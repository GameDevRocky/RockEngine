import math
import re
from collections import deque

from Domain import *


class AutoTiler(ScriptableComponent):
    """A 3x3 (9-piece) autotiling terrain painter, driven entirely by the mouse
    while the game is running.

    Left-click (and drag) in the game view to draw tiles on a grid anchored at
    this GameObject's position; right-click (and drag) to erase them. Every
    tile picks its sprite automatically from its 4 cardinal neighbors (corner /
    edge / center), gets a SpriteRenderer, a static RigidBody and a BoxCollider
    sized to the sprite -- so painted terrain is solid immediately.

    ── Editor setup ──────────────────────────────────────────────────────────
    Attach to an empty GameObject (the "tilemap root" -- painted tiles are
    parented under it, and it doubles as the grid's origin/anchor point).
    Assign the 9 `tile_*` sprite fields below to the matching pieces of your
    tileset (a corner/edge/center 3x3 blob set -- see e.g.
    `Domain/sandbox/assets/Mossy Tileset`). Set `cell_size` to match the pixel
    size of those sprites (e.g. 32 for 32x32 tiles) so painted tiles tile
    seamlessly with no gaps or overlap.

    `width`/`height` (in cells) are the BRUSH size: one click stamps a
    width x height block of tiles centred on the cell under the mouse, and
    right-click erases the same block. `1 x 1` (the default) is a single tile;
    `3 x 1` paints a 3-wide horizontal strip, and so on. Even sizes can't be
    perfectly centred, so the extra cell goes right/up. `show_brush` outlines the
    footprint at the mouse while playing, so you can see what a click will hit.

    Enter Play mode to paint. Press `save_key` (default: S) to write the
    current terrain back to the scene file on disk -- see `Scene.save()` for
    why that's allowed during Play mode here (normally saving only happens in
    Editor mode, since Play mode runs a throwaway copy of the world).

    Re-opening a saved scene keeps working: on `awake()` this rescans its own
    child GameObjects (matched by their "AutoTile_<x>_<y>" name) and rebuilds
    its internal grid from them, so painting/erasing continues seamlessly
    around previously-saved terrain instead of stacking new tiles on top.
    """

    # ── Tileset (3x3 / 9-piece autotile set) ─────────────────────────────────
    tile_top_left: Sprite = None
    tile_top: Sprite = None
    tile_top_right: Sprite = None
    tile_left: Sprite = None
    tile_center: Sprite = None
    tile_right: Sprite = None
    tile_bottom_left: Sprite = None
    tile_bottom: Sprite = None
    tile_bottom_right: Sprite = None

    # ── Grid / input ──────────────────────────────────────────────────────────
    cell_size: Reflect[float, Range(1.0, 4096.0), Step(1.0),
                        Tooltip("Grid cell size in pixels -- match your tile sprites' size.")] = 32.0
    width: Reflect[int, Range(1, 256), Step(1),
                    Tooltip("Brush width in cells -- how many tiles a single click paints/erases horizontally.")] = 1
    height: Reflect[int, Range(1, 256), Step(1),
                    Tooltip("Brush height in cells -- how many tiles a single click paints/erases vertically.")] = 1
    show_brush: Reflect[bool, Tooltip("Outline the brush footprint at the mouse while playing.")] = True
    save_key: Reflect[int, Tooltip("Qt keycode (see the Keys class) that saves the terrain to disk while in Play mode.")] = Keys.S

    # Cardinal-neighbor bitmask -> which tile_* field to use. Only 9 of the 16
    # possible 4-bit combinations have an exact piece; any other combination
    # (e.g. an isolated tile, or a straight 1-wide line) falls back to the
    # closest match by Hamming distance in `_field_for_mask`.
    _N, _E, _S, _W = 1, 2, 4, 8
    _MASK_TABLE = {
        _E | _S:            'tile_top_left',
        _W | _E | _S:       'tile_top',
        _W | _S:            'tile_top_right',
        _N | _E | _S:       'tile_left',
        _N | _E | _S | _W:  'tile_center',
        _N | _S | _W:       'tile_right',
        _N | _E:            'tile_bottom_left',
        _N | _W | _E:       'tile_bottom',
        _N | _W:            'tile_bottom_right',
    }
    _MASK_PRIORITY = list(_MASK_TABLE.keys())   # tie-break order for the fallback match
    _TILE_NAME_RE = re.compile(r"^AutoTile_(-?\d+)_(-?\d+)$")

    def awake(self):
        self._cells = {}             # (cx, cy) -> GameObject
        self._last_paint_cell = None
        self._last_erase_cell = None
        self._pending = deque()      # queued ('paint'|'erase', cell) tile edits
        self._worker_running = False
        self._rebuild_from_children()

    def update(self):
        if Input.is_key_pressed(self.save_key):
            self.save()

        cell = self._world_to_cell(Input.get_mouse_pos())

        if self.show_brush:
            self._draw_brush(cell)

        # Cell-edge-triggered, not frame-triggered: dragging paints once per new
        # cell entered, so holding the button still costs one stamp per cell.
        if Input.mouse_down(MouseButton.LEFT):
            if cell != self._last_paint_cell:
                self.paint_brush(cell)
                self._last_paint_cell = cell
        else:
            self._last_paint_cell = None

        if Input.mouse_down(MouseButton.RIGHT):
            if cell != self._last_erase_cell:
                self.erase_brush(cell)
                self._last_erase_cell = cell
        else:
            self._last_erase_cell = None

    # ── painting ──────────────────────────────────────────────────────────────
    # Every edit goes through a queue drained by ONE worker coroutine, exactly one
    # tile per frame. Building a tile means instantiating a GameObject plus a
    # SpriteRenderer, a static Rigidbody and a BoxCollider, so a big brush (or a
    # fast drag laying down a stamp per cell) would otherwise do hundreds of those
    # in a single frame and freeze on the click. One-per-frame means a click costs
    # a single tile and the rest streams in, at the cost of a wide brush taking as
    # many frames as it has cells to finish filling.
    #
    # A single worker -- rather than one coroutine per stamp -- keeps edits in the
    # order they were requested, so a paint and an erase of the same cell can never
    # resolve out of order, and the number of live coroutines stays at one no matter
    # how long the drag.
    def paint_brush(self, cell):
        """Queue the whole width x height brush centred on `cell` for painting."""
        self._enqueue('paint', self._brush_cells(cell))

    def erase_brush(self, cell):
        """Queue every cell under the width x height brush centred on `cell` for erasing."""
        self._enqueue('erase', self._brush_cells(cell))

    def place_tile(self, cell):
        """Queue a single tile at grid cell `cell` (a (cx, cy) int tuple),
        ignoring the brush size. No-op if the cell is already occupied."""
        self._enqueue('paint', [cell])

    def erase_tile(self, cell):
        """Queue the single tile at grid cell `cell` for erasing, if any."""
        self._enqueue('erase', [cell])

    def _enqueue(self, op, cells):
        for c in cells:
            self._pending.append((op, c))
        if not self._worker_running:
            self._worker_running = True
            # start_coroutine runs the body up to the first yield immediately, so
            # one tile lands on this frame -- a click still shows something at once.
            self.start_coroutine(self._drain_pending())

    def _drain_pending(self):
        """Worker coroutine: apply ONE tile edit, then hand the frame back."""
        try:
            while self._pending:
                op, cell = self._pending.popleft()
                changed = self._create_tile(cell) if op == 'paint' else self._remove_tile(cell)
                # A no-op (painting an occupied cell, erasing an empty one) did no
                # work, so don't spend a frame on it -- re-stamping over existing
                # terrain drains in one go instead of crawling a frame per cell.
                if not changed:
                    continue
                # Re-pick this cell's and its neighbours' sprites now, so the
                # terrain on screen stays correct as it streams in.
                self._refresh_area([cell])
                # Only sleep if work remains: the final edit (and a single-tile stamp
                # outright) then completes inside start_coroutine's first next(), so
                # no coroutine is registered and there's no idle wind-down frame.
                if self._pending:
                    yield WaitForEndOfFrame()
        finally:
            # Also runs if the coroutine is discarded (stop_all_coroutines, or the
            # object being torn down), so the queue can always restart.
            self._worker_running = False

    def _create_tile(self, cell):
        """Build the tile GameObject for `cell` -- sprite, static body, collider.
        Returns True if one was created. Deliberately does NOT re-pick neighbour
        sprites; callers batch that through `_refresh_area`."""
        if cell in self._cells:
            return False

        cx, cy = cell
        obj = self.instantiate(f"AutoTile_{cx}_{cy}")
        if not obj:
            return False
        obj.transform.parent = self.transform
        obj.transform.world_position = self._cell_to_world(cell)

        sr = obj.add_component(SpriteRenderer)
        self._apply_sprite(obj, sr, self._sprite_for_mask(self._compute_mask(cell)))

        rb = obj.add_component(Rigidbody)
        rb.body_type = Rigidbody.STATIC

        # Added last: BoxCollider auto-sizes itself from the SpriteRenderer's
        # sprite (in pixels) the moment it's attached, then gets multiplied by
        # the transform.scale _apply_sprite just set -- so the physics footprint
        # ends up exactly cell_size, matching the (rescaled) rendered sprite.
        obj.add_component(BoxCollider)

        self._cells[cell] = obj
        return True

    def _remove_tile(self, cell):
        """Destroy the tile at `cell`. Returns True if one was there. Like
        `_create_tile`, leaves neighbour sprites to `_refresh_area`."""
        obj = self._cells.pop(cell, None)
        if obj is None:
            return False
        obj.destroy()
        return True

    def save(self):
        """Persist the current terrain to the scene file on disk."""
        if Scene.save(self):
            Console.comment(f"[AutoTiler] Saved scene ({len(self._cells)} tiles).")
        else:
            Console.warn("[AutoTiler] Could not save -- this scene has no file path yet.")

    # ── autotiling ────────────────────────────────────────────────────────────
    def _compute_mask(self, cell):
        cx, cy = cell
        mask = 0
        if (cx, cy + 1) in self._cells: mask |= self._N
        if (cx + 1, cy) in self._cells: mask |= self._E
        if (cx, cy - 1) in self._cells: mask |= self._S
        if (cx - 1, cy) in self._cells: mask |= self._W
        return mask

    def _field_for_mask(self, mask):
        field = self._MASK_TABLE.get(mask)
        if field is not None:
            return field
        best = min(self._MASK_PRIORITY, key=lambda m: bin(mask ^ m).count('1'))
        return self._MASK_TABLE[best]

    def _sprite_for_mask(self, mask):
        return getattr(self, self._field_for_mask(mask))

    def _apply_sprite(self, obj, sr, sprite):
        """Assign `sprite` to `sr` and rescale `obj` so the rendered sprite
        exactly fills one grid cell, regardless of the sprite's native pixel
        size (assumes all 9 tile_* sprites share the same pixel dimensions --
        true for any normal tileset)."""
        if not sprite:
            Console.warn("[AutoTiler] No sprite assigned for this tile shape -- "
                          "assign the tile_* fields in the inspector.")
            return
        sr.sprite = sprite
        obj.transform.scale = self._fit_scale(sprite)

    def _fit_scale(self, sprite):
        size = self._safe_cell_size()
        pixel_size = sprite.pixel_size
        sx = size / pixel_size.x if pixel_size.x > 0.0 else 1.0
        sy = size / pixel_size.y if pixel_size.y > 0.0 else 1.0
        return Vector2(sx, sy)

    def _refresh_cell_sprite(self, cell):
        obj = self._cells.get(cell)
        if not obj:
            return
        sr = obj.get_component(SpriteRenderer)
        if not sr:
            return
        self._apply_sprite(obj, sr, self._sprite_for_mask(self._compute_mask(cell)))

    def _refresh_area(self, cells):
        """Re-pick sprites for `cells` plus everything cardinally adjacent, in one
        pass over the de-duplicated union. A mask only depends on a cell's four
        neighbours, so adding/removing a cell can only change that cell's own mask
        and its neighbours' -- making this union exact. The worker calls this with a
        single cell per frame; it stays list-shaped so a caller that changes several
        cells at once still pays only one refresh per affected cell."""
        if not cells:
            return
        affected = set()
        for (cx, cy) in cells:
            affected.update((
                (cx, cy), (cx, cy + 1), (cx + 1, cy), (cx, cy - 1), (cx - 1, cy)))
        for c in affected:
            self._refresh_cell_sprite(c)   # no-ops on empty cells

    # ── brush ─────────────────────────────────────────────────────────────────
    def _brush_rect(self, cell):
        """(x0, y0, w, h) of the brush centred on `cell`. An even width/height
        can't be centred exactly, so the extra cell goes right/up. Sizes are
        clamped to >= 1 so a 0 typed into the inspector still paints one tile
        rather than nothing."""
        cx, cy = cell
        w = max(1, self.width)
        h = max(1, self.height)
        return cx - (w - 1) // 2, cy - (h - 1) // 2, w, h

    def _brush_cells(self, cell):
        """Every cell the brush covers when centred on `cell`."""
        x0, y0, w, h = self._brush_rect(cell)
        return [(x0 + dx, y0 + dy) for dy in range(h) for dx in range(w)]

    def _draw_brush(self, cell):
        """Outline the brush footprint at the mouse, so a click's extent is
        visible before committing to it."""
        x0, y0, w, h = self._brush_rect(cell)
        size = self._safe_cell_size()
        span = Vector2(w * size, h * size)
        # _cell_to_world gives the lower-left cell's *centre*; step to the rect's
        # centre by half the span, less the half cell already included.
        centre = self._cell_to_world((x0, y0)) + (span - Vector2(size, size)) * 0.5
        Debug.draw_box(centre, span, 0.0, (0.2, 0.9, 1.0, 1.0))

    # ── grid <-> world ────────────────────────────────────────────────────────
    def _safe_cell_size(self):
        return self.cell_size if self.cell_size > 0.0 else 1.0

    def _world_to_cell(self, world_pos):
        origin = self.transform.world_position
        size = self._safe_cell_size()
        local = world_pos - origin
        return (math.floor(local.x / size), math.floor(local.y / size))

    def _cell_to_world(self, cell):
        origin = self.transform.world_position
        size = self._safe_cell_size()
        cx, cy = cell
        return origin + Vector2((cx + 0.5) * size, (cy + 0.5) * size)

    # ── persistence continuity ───────────────────────────────────────────────
    def _rebuild_from_children(self):
        """Repopulate the grid from tiles already parented under this object
        (e.g. loaded from a previously-saved scene) so editing can continue
        without re-stacking tiles on top of existing ones."""
        for child in self.transform.children:
            match = self._TILE_NAME_RE.match(child.gameobject.name)
            if not match:
                continue
            cell = (int(match.group(1)), int(match.group(2)))
            self._cells[cell] = child.gameobject
        # A hand-edited scene could leave stale neighbor masks; resync once.
        for cell in list(self._cells):
            self._refresh_cell_sprite(cell)
