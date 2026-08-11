"""Wolfenstein-style raycaster rendered entirely with the Debug draw API.

Attach to any GameObject and press play. The object's world position is the
CENTRE of the virtual screen; everything is drawn as debug geometry in world
space around it, so pan/zoom the Scene view to frame it.

  Controls   W/Up, S/Down  walk        A/Left, D/Right  turn        Q, E  strafe

Only the editor Scene view runs a DebugPass (EditorRenderView::Init), so this
draws in the Scene view -- including while in play mode -- and not in the Game
view.

── How it works ────────────────────────────────────────────────────────────
Classic grid DDA, one ray per screen column:
  * the camera is a direction vector `dir` plus a `plane` vector perpendicular
    to it whose length is tan(fov/2); column i shoots along dir + plane*camX
    with camX in [-1, 1],
  * DDA steps cell-to-cell until it lands on a non-blank map character; the
    accumulated side distance is already the PERPENDICULAR distance to the
    camera plane, which is what removes fisheye distortion (a normalised
    euclidean distance would bow the walls),
  * wall height on screen is proportional to 1 / that distance.

Map coordinates are (column, row) into `map_rows`, so map +y runs DOWN the text
as written. The minimap flips y back when drawing so it reads like the source.

── Cost ────────────────────────────────────────────────────────────────────
Each wall column is one filled polygon, and DebugPass issues filled polygons
immediately -- two GL draws apiece, no batching. `columns` is the dial: 120 is
~240 draws/frame, comfortable, but drop it if you stack several of these. The
minimap is nearly free by comparison: its cells and rays are boxes and lines,
which DebugPass instances into one draw call each.
"""

import math

from Domain import *


# ── Palette ─────────────────────────────────────────────────────────────────
# Map character -> wall colour. Any other non-blank character falls back to
# _WALL_FALLBACK, so new digits can go straight into map_rows without a code
# change.
_WALL_COLORS = {
    '1': (0.74, 0.26, 0.24),   # brick
    '2': (0.26, 0.44, 0.78),   # blue stone
    '3': (0.30, 0.64, 0.38),   # moss
    '4': (0.82, 0.68, 0.24),   # gold
}
_WALL_FALLBACK = (0.70, 0.70, 0.70)
_BLANK_CHARS   = ' .0'         # characters that count as open floor

_CEILING_COLOR = (0.15, 0.16, 0.21)
_FLOOR_COLOR   = (0.25, 0.21, 0.17)
_FRAME_COLOR   = (0.90, 0.90, 0.95, 1.0)
_MM_WALL_COLOR   = (0.55, 0.56, 0.64, 1.0)
_MM_BORDER_COLOR = (0.85, 0.86, 0.92, 1.0)
_MM_RAY_COLOR    = (0.95, 0.78, 0.25, 0.35)
_MM_PLAYER_COLOR = (0.30, 1.00, 0.45, 1.0)

# DebugPass draws a filled polygon twice: the outline in the colour we hand it,
# then the interior at alpha*0.5. Doubling the alpha lands the interior on the
# opacity we actually want, and GL clamps blend factors to 1 so the outline
# stays opaque rather than overflowing. Outline and fill share an RGB, so
# neighbouring columns show no seam.
_FILL_ALPHA = 2.0

_SIDE_DIM  = 0.65   # classic Wolf3D look: horizontal wall faces are darker
_MIN_SHADE = 0.12   # walls fade towards this, never to pure black
_MAX_DDA_STEPS = 64


def _shaded(rgb, k):
    """Wall colour scaled by brightness k, at fill-compensated alpha."""
    return (rgb[0] * k, rgb[1] * k, rgb[2] * k, _FILL_ALPHA)


def _fill_quad(x0, y0, x1, y1, color):
    """Axis-aligned filled rect. Corner order is CCW so the GL_TRIANGLE_FAN
    DebugPass builds from it covers the rect."""
    Debug.draw_points(
        [Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1)],
        closed=True, filled=True, color=color)


class Raycaster(ScriptableComponent):

    # ── Virtual screen (world units, centred on this GameObject) ────────────
    screen_width:  Reflect[float, Range(64, 4096), Step(16)] = 640.0
    screen_height: Reflect[float, Range(64, 4096), Step(16)] = 400.0
    columns:       Reflect[int, Range(8, 480), Step(1),
                           Tooltip("One filled quad per column -- the main cost knob.")] = 120
    fov:           Reflect[float, Range(20, 140), Step(1)] = 66.0
    wall_scale:    Reflect[float, Range(0.1, 4.0)] = 1.0
    fog_distance:  Reflect[float, Range(0.0, 64.0), Step(0.5),
                           Tooltip("Cells until a wall reaches minimum brightness. 0 = no falloff.")] = 14.0

    # ── Movement (cells/sec, degrees/sec) ───────────────────────────────────
    move_speed:    Reflect[float, Range(0.0, 20.0)] = 3.0
    turn_speed:    Reflect[float, Range(0.0, 720.0), Step(5)] = 120.0
    player_radius: Reflect[float, Range(0.0, 0.45), Step(0.01)] = 0.2

    # ── Minimap ─────────────────────────────────────────────────────────────
    show_minimap: bool = True
    minimap_cell: Reflect[float, Range(1.0, 64.0)] = 14.0
    ray_stride:   Reflect[int, Range(1, 32), Step(1),
                          Tooltip("Draw every Nth ray on the minimap.")] = 4

    # ── Spawn ───────────────────────────────────────────────────────────────
    start_x:     Reflect[float, Step(0.5)] = 1.5
    start_y:     Reflect[float, Step(0.5)] = 7.5
    start_angle: Reflect[float, Range(0.0, 360.0), Step(5)] = 0.0

    # Blank (or '.'/'0') is floor; any other character is a wall keyed into
    # _WALL_COLORS. Rows need not be equal length -- anything past the end of a
    # row, or outside the grid, reads as solid.
    map_rows: list[str] = [
        "1111111111111111",
        "1              1",
        "1 2222   3333  1",
        "1 2      3     1",
        "1 2  44  3  33 1",
        "1    44        1",
        "1  3333   222  1",
        "1              1",
        "1  44     2222 1",
        "1  44          1",
        "1     3333  33 1",
        "1  22    3     1",
        "1  22    3  44 1",
        "1        3     1",
        "1  222222   44 1",
        "1111111111111111",
    ]

    # ── Lifecycle ───────────────────────────────────────────────────────────
    def awake(self):
        self.pos_x = float(self.start_x)
        self.pos_y = float(self.start_y)
        self.angle = float(self.start_angle)

    def update(self):
        dt = Time.delta_time
        self._move(dt)
        self._render()

    # ── Map access ──────────────────────────────────────────────────────────
    def _tile(self, cell_x, cell_y):
        """Map character at a cell, or '1' outside the grid. Returns '' for
        open floor so callers can just test truthiness. Reads map_rows live, so
        edits in the inspector take effect without a restart, and a ragged or
        empty map is closed off rather than crashing."""
        rows = self.map_rows
        if cell_y < 0 or cell_y >= len(rows):
            return '1'
        row = rows[cell_y]
        if cell_x < 0 or cell_x >= len(row):
            return '1'
        char = row[cell_x]
        return '' if char in _BLANK_CHARS else char

    def _is_wall(self, cell_x, cell_y):
        return bool(self._tile(cell_x, cell_y))

    # ── Movement ────────────────────────────────────────────────────────────
    def _move(self, dt):
        turn = self._held(Keys.RIGHT, Keys.D) - self._held(Keys.LEFT, Keys.A)
        # Map y points down, so a clockwise turn on the page (which reads as
        # turning right) is an INCREASING angle.
        self.angle = (self.angle + turn * self.turn_speed * dt) % 360.0

        rad = math.radians(self.angle)
        dir_x, dir_y = math.cos(rad), math.sin(rad)

        forward = self._held(Keys.UP, Keys.W) - self._held(Keys.DOWN, Keys.S)
        strafe = self._held(Keys.E) - self._held(Keys.Q)
        if forward == 0.0 and strafe == 0.0:
            return

        # Right-hand vector in a y-down frame is (-dir_y, dir_x).
        step = self.move_speed * dt
        self._step_axis((dir_x * forward - dir_y * strafe) * step, 0.0)
        self._step_axis(0.0, (dir_y * forward + dir_x * strafe) * step)

    def _step_axis(self, dx, dy):
        """Move on one axis only, refusing the step if the player's disc would
        end up inside a wall. Doing x and y as separate calls is what makes the
        player slide along a wall instead of sticking to it."""
        if dx == 0.0 and dy == 0.0:
            return
        radius = self.player_radius
        new_x = self.pos_x + dx
        new_y = self.pos_y + dy

        if dx != 0.0:
            edge = math.floor(new_x + math.copysign(radius, dx))
            # Test both ends of the player's y extent, or a corner clips through.
            if (self._is_wall(edge, math.floor(self.pos_y - radius)) or
                    self._is_wall(edge, math.floor(self.pos_y + radius))):
                return
        else:
            edge = math.floor(new_y + math.copysign(radius, dy))
            if (self._is_wall(math.floor(self.pos_x - radius), edge) or
                    self._is_wall(math.floor(self.pos_x + radius), edge)):
                return

        self.pos_x = new_x
        self.pos_y = new_y

    # ── Raycast ─────────────────────────────────────────────────────────────
    def _cast(self, ray_x, ray_y):
        """DDA from the player along an (unnormalised) ray direction.

        Returns (perp_distance, side, tile) or None if nothing was hit inside
        _MAX_DDA_STEPS. `side` is 0 for a vertical wall face, 1 for horizontal.
        The distance is in units of the ray parameter t: because the ray is
        dir + plane*camX rather than a unit vector, t is already the distance
        measured along the view axis -- i.e. fisheye-free, no cosine fixup."""
        cell_x = math.floor(self.pos_x)
        cell_y = math.floor(self.pos_y)

        # Ray length needed to cross one full cell on each axis. A ray exactly
        # parallel to an axis never crosses the other one; 1e30 keeps that side
        # permanently losing the comparison below without a special case.
        delta_x = abs(1.0 / ray_x) if ray_x != 0.0 else 1e30
        delta_y = abs(1.0 / ray_y) if ray_y != 0.0 else 1e30

        if ray_x < 0.0:
            step_x, side_dist_x = -1, (self.pos_x - cell_x) * delta_x
        else:
            step_x, side_dist_x = 1, (cell_x + 1.0 - self.pos_x) * delta_x
        if ray_y < 0.0:
            step_y, side_dist_y = -1, (self.pos_y - cell_y) * delta_y
        else:
            step_y, side_dist_y = 1, (cell_y + 1.0 - self.pos_y) * delta_y

        for _ in range(_MAX_DDA_STEPS):
            # Always advance across whichever grid line is nearer.
            if side_dist_x < side_dist_y:
                side_dist_x += delta_x
                cell_x += step_x
                side = 0
            else:
                side_dist_y += delta_y
                cell_y += step_y
                side = 1

            tile = self._tile(cell_x, cell_y)
            if tile:
                # Back off the increment we just made: the crossing itself is
                # the hit, not the next grid line.
                dist = (side_dist_x - delta_x) if side == 0 else (side_dist_y - delta_y)
                return max(dist, 1e-4), side, tile
        return None

    # ── Render ──────────────────────────────────────────────────────────────
    def _render(self):
        origin = self.transform.world_position
        width  = self.screen_width
        height = self.screen_height
        left   = origin.x - width * 0.5
        bottom = origin.y - height * 0.5
        top    = origin.y + height * 0.5
        horizon = origin.y

        _fill_quad(left, horizon, left + width, top, _shaded(_CEILING_COLOR, 1.0))
        _fill_quad(left, bottom, left + width, horizon, _shaded(_FLOOR_COLOR, 1.0))

        rad = math.radians(self.angle)
        dir_x, dir_y = math.cos(rad), math.sin(rad)
        # Camera plane: perpendicular to dir, half-width tan(fov/2). +camX must
        # land on screen-right, which in a y-down frame is (-dir_y, dir_x).
        half_fov = math.tan(math.radians(self.fov) * 0.5)
        plane_x, plane_y = -dir_y * half_fov, dir_x * half_fov

        count = max(1, int(self.columns))
        col_width = width / count
        # Half-height of a unit-tall wall standing one cell away.
        projection = height * 0.5 * self.wall_scale
        fog = self.fog_distance

        hits = []
        for i in range(count):
            camera_x = 2.0 * (i + 0.5) / count - 1.0
            ray_x = dir_x + plane_x * camera_x
            ray_y = dir_y + plane_y * camera_x

            hit = self._cast(ray_x, ray_y)
            if hit is None:
                hits.append(None)
                continue
            dist, side, tile = hit
            hits.append((ray_x, ray_y, dist))

            half = projection / dist
            y0 = max(bottom, horizon - half)
            y1 = min(top, horizon + half)
            if y1 <= y0:
                continue    # wall slice entirely off-screen

            shade = 1.0 if side == 0 else _SIDE_DIM
            if fog > 0.0:
                shade *= max(_MIN_SHADE, 1.0 - dist / fog)
            x0 = left + i * col_width
            _fill_quad(x0, y0, x0 + col_width, y1,
                       _shaded(_WALL_COLORS.get(tile, _WALL_FALLBACK), shade))

        # Boxes are instanced and drawn after every polygon, so the frame always
        # lands on top of the wall columns.
        Debug.draw_box(Vector2(origin.x, origin.y), Vector2(width, height),
                       0.0, _FRAME_COLOR)

        if self.show_minimap:
            self._draw_minimap(origin, hits)

    def _draw_minimap(self, origin, hits):
        cell = self.minimap_cell
        rows = self.map_rows
        if not rows or cell <= 0.0:
            return
        cols = max(len(r) for r in rows)

        # Top-left of the map, just right of the virtual screen.
        map_left = origin.x + self.screen_width * 0.5 + cell * 1.5
        map_top  = origin.y + self.screen_height * 0.5

        def to_world(map_x, map_y):
            # Flip y so row 0 of map_rows sits at the top, as written.
            return Vector2(map_left + map_x * cell, map_top - map_y * cell)

        for row_index, row in enumerate(rows):
            for col_index, char in enumerate(row):
                if char in _BLANK_CHARS:
                    continue
                color = _WALL_COLORS.get(char)
                center = to_world(col_index + 0.5, row_index + 0.5)
                Debug.draw_box(center, Vector2(cell, cell), 0.0,
                               (color + (1.0,)) if color else _MM_WALL_COLOR)

        Debug.draw_box(to_world(cols * 0.5, len(rows) * 0.5),
                       Vector2(cols * cell, len(rows) * cell), 0.0, _MM_BORDER_COLOR)

        player = to_world(self.pos_x, self.pos_y)
        stride = max(1, int(self.ray_stride))
        for i in range(0, len(hits), stride):
            hit = hits[i]
            if hit is None:
                continue
            ray_x, ray_y, dist = hit
            # pos + raydir * t is the exact hit point: t came out of the DDA in
            # units of this same unnormalised direction.
            Debug.draw_line(player,
                            to_world(self.pos_x + ray_x * dist,
                                     self.pos_y + ray_y * dist),
                            _MM_RAY_COLOR)

        rad = math.radians(self.angle)
        Debug.draw_circle(player, cell * 0.25, _MM_PLAYER_COLOR)
        Debug.draw_line(player,
                        to_world(self.pos_x + math.cos(rad) * 0.9,
                                 self.pos_y + math.sin(rad) * 0.9),
                        _MM_PLAYER_COLOR)

    # ── Helpers ─────────────────────────────────────────────────────────────
    @staticmethod
    def _held(*keys):
        return 1.0 if any(Input.is_key_down(k) for k in keys) else 0.0
