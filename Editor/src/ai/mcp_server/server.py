"""MCP server exposing a running RockEngine editor as tools.

Claude Code spawns this over stdio. It holds no engine logic: each tool forwards one
JSON-RPC call to the editor's in-process bridge (Editor/src/mcp/) over a local pipe and
returns whatever came back. If the editor is not running it is launched on demand.

Every result carries `worldMode`. When that is "Runtime" the edit landed on play mode's
deep-copied world and will be discarded on Stop -- the accompanying `warning` says so.
"""

from __future__ import annotations

import asyncio
from typing import Any

# mcp 2.x. This was `from mcp.server.fastmcp import FastMCP` in 1.x -- the class was
# renamed and moved, so a 1.x install fails at import rather than misbehaving later.
from mcp.server.mcpserver import MCPServer

from editor_bridge import EditorBridge, EditorRpcError

mcp = MCPServer("rockengine")

_bridge = EditorBridge()
# One pipe, strictly request/response: without this two concurrent tool calls could
# interleave writes and each read the other's reply.
_lock = asyncio.Lock()


async def call(method: str, **params: Any) -> Any:
    """Forward one call to the editor, off the event loop so a slow reply cannot stall it."""
    async with _lock:
        try:
            return await asyncio.to_thread(_bridge.call, method, params)
        except EditorRpcError as e:
            return {"error": e.message, "code": e.code}
        except (ConnectionError, OSError) as e:
            return {"error": f"could not reach the RockEngine editor: {e}"}


def _editor_data(result: Any) -> dict[str, Any]:
    """Unwrap the editor bridge's {data, worldMode} result for local orchestration."""
    if isinstance(result, dict) and isinstance(result.get("data"), dict):
        return result["data"]
    return result if isinstance(result, dict) else {}


async def _wait_for_clarification(request_id: str, timeout_seconds: int = 300) -> Any:
    """Poll without holding the editor pipe or blocking its event loop."""
    timeout_seconds = max(5, min(timeout_seconds, 900))
    deadline = asyncio.get_running_loop().time() + timeout_seconds
    while asyncio.get_running_loop().time() < deadline:
        result = await call("user.clarification_status", requestId=request_id)
        data = _editor_data(result)
        if data.get("status") != "pending":
            return result
        await asyncio.sleep(0.25)
    return {
        "error": "user clarification timed out",
        "requestId": request_id,
        "status": "timed_out",
    }


# --- User clarification -------------------------------------------------------------

@mcp.tool()
async def ask_user_clarification(
    question: str,
    options: list[dict[str, str]],
    context: str = "",
    allow_multiple: bool = False,
    title: str = "Clarification needed",
    timeout_seconds: int = 300,
) -> Any:
    """Ask the user an in-editor clarification question and wait for their answer.

    Call this before acting when required information is missing, several materially
    different interpretations are plausible, or the operation has important side
    effects the user did not explicitly acknowledge. Each option requires an `id` and
    `label`; an optional `description` should explain its consequence or next step.
    Set allow_multiple=True only when choices are independent and can be combined.
    The editor always appends an Other option with free-form input.
    """
    started = await call(
        "user.request_clarification",
        title=title,
        question=question,
        context=context,
        options=options,
        allowMultiple=allow_multiple,
    )
    request_id = _editor_data(started).get("requestId", "")
    if not request_id:
        return started
    return await _wait_for_clarification(request_id, timeout_seconds)


# --- Scene and hierarchy ------------------------------------------------------------

@mcp.tool()
async def list_scenes() -> Any:
    """List the scenes currently loaded in the editor."""
    return await call("scene.list")


@mcp.tool()
async def load_scene(path: str) -> Any:
    """Load a .scene file, e.g. "Domain/sandbox/default.scene". Relative paths resolve
    against the asset root. Asynchronous -- poll list_scenes to see it appear.

    The editor opens with no scene loaded, so this is usually the first call needed.
    """
    return await call("scene.load", path=path)


@mcp.tool()
async def create_scene(name: str = "", directory: str = "") -> Any:
    """Create a new scene: writes a minimal valid .scene file to disk (name field, a
    fresh id, empty gameobjects/components), like Unity's Assets > Create > Scene, then
    loads it into the editor the same way dragging that file onto the hierarchy would.

    name defaults to "New Scene"; directory defaults to "Domain/sandbox" (where every
    other scene in the repo lives). Both a relative path (asset-root-relative) and an
    absolute path are accepted for directory. Collides with an existing file the same
    way the Folder view's New menu does -- "New Scene 1.scene", "New Scene 2.scene", ...
    Asynchronous -- poll list_scenes to see it appear.
    """
    return await call("scene.new", name=name, directory=directory)


@mcp.tool()
async def unload_scene(scene_id: str = "") -> Any:
    """Drop a loaded scene from the editor's live world without touching its file on
    disk. Defaults to the only loaded scene. Asynchronous -- poll list_scenes to see
    it gone."""
    return await call("scene.unload", sceneId=scene_id)


@mcp.tool()
async def save_scene(scene_id: str = "") -> Any:
    """Save a loaded scene back to its file. Defaults to the only loaded scene.
    Refused during play mode."""
    return await call("scene.save", sceneId=scene_id)


@mcp.tool()
async def get_hierarchy(scene_id: str = "", max_depth: int = 0) -> Any:
    """Get the GameObject tree. Omit scene_id for every loaded scene.

    max_depth is the number of levels returned: 1 = roots only, 2 = roots plus their
    children, 0 = unlimited. Start with max_depth=1 on an unfamiliar scene — tilemap
    parents can hold hundreds of children. Nodes whose children were cut still report
    childCount, so you know where to look deeper.
    """
    return await call("scene.hierarchy", sceneId=scene_id, maxDepth=max_depth)


@mcp.tool()
async def get_object(object_id: str) -> Any:
    """Get one GameObject's name, tag, active state, components and transform."""
    return await call("object.get_details", id=object_id)


@mcp.tool()
async def select_object(object_id: str = "") -> Any:
    """Select a GameObject in the editor, so the Hierarchy and Inspector follow it.
    Pass no id to clear the selection."""
    return await call("object.select", id=object_id)


@mcp.tool()
async def rename_object(object_id: str, name: str) -> Any:
    """Rename a GameObject."""
    return await call("object.set_name", id=object_id, value=name)


@mcp.tool()
async def set_object_tag(object_id: str, tag: str) -> Any:
    """Set a GameObject's tag."""
    return await call("object.set_tag", id=object_id, value=tag)


@mcp.tool()
async def set_object_active(object_id: str, active: bool) -> Any:
    """Activate or deactivate a GameObject (and its children)."""
    return await call("object.set_active", id=object_id, value=active)


@mcp.tool()
async def set_parent(object_id: str, parent_id: str = "") -> Any:
    """Reparent a GameObject. Omit parent_id to move it to the scene root.

    Newly created objects always land at the scene root, so this is how you nest them.
    """
    return await call("object.set_parent", id=object_id, parentId=parent_id)


@mcp.tool()
async def remove_component(object_id: str, component_id: str, confirm: bool = False) -> Any:
    """Detach an exact component after mandatory in-editor impact clarification.

    RockEngine calculates cascaded removals and behavioral effects first. For example,
    removing a RigidBody lists its Collider shapes and own/connected Joints. The user
    can approve removal, disable instead, inspect affected IDs, cancel, or choose Other.
    `confirm` is retained for older clients but cannot bypass the user answer bank.
    Transform cannot be removed.
    """
    _ = confirm
    started = await call("object.remove_component", id=object_id, componentId=component_id)
    impact = _editor_data(started)
    if impact.get("status") != "clarification_required":
        return started
    request_id = impact.get("requestId", "")
    answered_result = await _wait_for_clarification(request_id)
    answer = _editor_data(answered_result)
    selected = set(answer.get("selectedIds", []))
    if answer.get("status") == "answered" and "remove" in selected:
        return await call("object.remove_component", id=object_id,
                          componentId=component_id, clarificationId=request_id)
    if answer.get("status") == "answered" and "disable" in selected:
        result = await call("component.set_property", componentId=component_id,
                            property="enabled", value=False)
        return {"status": "alternative_applied", "choice": "disable",
                "result": result, "clarification": answer, "affected": impact.get("affected", [])}
    return {"status": "not_removed", "choice": next(iter(selected), "cancel"),
            "clarification": answer, "affected": impact.get("affected", []),
            "otherText": answer.get("otherText", "")}


@mcp.tool()
async def dump_object_state(object_id: str) -> Any:
    """Dump a GameObject's complete serialized state, including every component field.
    Use when a specific getter does not exist for what you need."""
    return await call("object.dump_state", id=object_id)


# --- Transform ---------------------------------------------------------------------

@mcp.tool()
async def get_position(object_id: str, world: bool = False) -> Any:
    """Get a GameObject's position (local by default, world-space if world=True)."""
    return await call(f"transform.get_{'world_position' if world else 'position'}", id=object_id)


@mcp.tool()
async def set_position(object_id: str, x: float, y: float, world: bool = False) -> Any:
    """Move a GameObject to (x, y), local by default or world-space if world=True."""
    return await call(f"transform.set_{'world_position' if world else 'position'}",
                      id=object_id, x=x, y=y)


@mcp.tool()
async def get_rotation(object_id: str, world: bool = False) -> Any:
    """Get a GameObject's rotation in degrees."""
    return await call(f"transform.get_{'world_rotation' if world else 'rotation'}", id=object_id)


@mcp.tool()
async def set_rotation(object_id: str, degrees: float, world: bool = False) -> Any:
    """Set a GameObject's rotation in degrees."""
    return await call(f"transform.set_{'world_rotation' if world else 'rotation'}",
                      id=object_id, value=degrees)


@mcp.tool()
async def get_scale(object_id: str, world: bool = False) -> Any:
    """Get a GameObject's scale."""
    return await call(f"transform.get_{'world_scale' if world else 'scale'}", id=object_id)


@mcp.tool()
async def set_scale(object_id: str, x: float, y: float, world: bool = False) -> Any:
    """Set a GameObject's scale."""
    return await call(f"transform.set_{'world_scale' if world else 'scale'}",
                      id=object_id, x=x, y=y)


# --- Assets ------------------------------------------------------------------------

@mcp.tool()
async def list_assets(asset_type: str = "", name_contains: str = "", limit: int = 50) -> Any:
    """List loaded assets with their ids, names and file paths. asset_type is one of
    sprite, material, texture, font, audio, shader; omit for everything but shaders.

    Asset ids are not guessable from filenames, so call this before assigning one.
    Results are capped (a sliced tileset yields hundreds of sprites) and each category
    reports `total` vs `returned` — filter with name_contains, e.g. "Terrain" or "Frog".
    """
    return await call("assets.list", type=asset_type, nameContains=name_contains, limit=limit)


@mcp.tool()
async def describe_asset(asset_id: str) -> Any:
    """Describe one loaded asset by exact id, including every MCP-editable property,
    its current value, type, enum choices, range, reference target and writeability."""
    return await call("asset.describe", assetId=asset_id)


@mcp.tool()
async def get_asset_property(asset_id: str, property_name: str) -> Any:
    """Read one discovered asset property. Call describe_asset for valid names."""
    return await call("asset.get_property", assetId=asset_id, property=property_name)


@mcp.tool()
async def set_asset_property(asset_id: str, property_name: str, value: Any) -> Any:
    """Set one discovered asset property through the same Resource setter used by the
    Inspector. Existing asset auto-save therefore persists the edit to its meta file."""
    return await call("asset.set_property", assetId=asset_id,
                      property=property_name, value=value)


@mcp.tool()
async def set_asset_properties(asset_id: str, values: dict[str, Any]) -> Any:
    """Atomically set several properties on one asset. On validation failure, all
    earlier writes are rolled back. Assets remain non-undoable, matching the Inspector."""
    return await call("asset.set_properties", assetId=asset_id, values=values)


@mcp.tool()
async def save_asset(asset_id: str) -> Any:
    """Explicitly persist a loaded asset's current in-memory state. Property setters
    normally auto-save already; this is useful as a synchronization checkpoint."""
    return await call("asset.save", assetId=asset_id)


@mcp.tool()
async def resolve_reference(reference_id: str) -> Any:
    """Resolve an exact GameObject, component, or loaded asset id into metadata. This
    is the shared reference format used by component and asset property schemas."""
    return await call("reference.resolve", id=reference_id)


@mcp.tool()
async def set_sprite(object_id: str, sprite_id: str = "") -> Any:
    """Assign a sprite to a SpriteRenderer by asset id (from list_assets).
    Pass no sprite_id to clear it."""
    return await call("sprite_renderer.set_sprite", id=object_id, spriteId=sprite_id)


@mcp.tool()
async def get_sprite(object_id: str) -> Any:
    """Read the sprite asset id currently on a SpriteRenderer."""
    return await call("sprite_renderer.get_sprite", id=object_id)


@mcp.tool()
async def set_audio_clip(object_id: str, clip_id: str = "") -> Any:
    """Assign an audio clip to an AudioSource by asset id (from list_assets)."""
    return await call("audio_source.set_clip", id=object_id, clipId=clip_id)


# --- Components --------------------------------------------------------------------

@mcp.tool()
async def get_capabilities() -> Any:
    """Discover bridge version, registered component types, generic tools, exact-id
    addressing, transaction support and undo behavior."""
    return await call("capabilities.get")


@mcp.tool()
async def list_components(object_id: str) -> Any:
    """List every component on a GameObject in serialized order. Each entry includes
    its exact component id and full property schema. Use exact ids for repeatable
    components such as colliders, joints, scripts and audio sources."""
    return await call("component.list", objectId=object_id)


@mcp.tool()
async def describe_component(component_id: str) -> Any:
    """Describe one exact component, including all current values, enum choices,
    numeric constraints, asset/object reference types and supported actions."""
    return await call("component.describe", componentId=component_id)


@mcp.tool()
async def get_component_property(component_id: str, property_name: str) -> Any:
    """Read one property from an exact component id."""
    return await call("component.get_property", componentId=component_id,
                      property=property_name)


@mcp.tool()
async def set_component_property(component_id: str, property_name: str, value: Any) -> Any:
    """Set one property on an exact component id. The edit uses the engine setter and
    is recorded in the active world's Undo stack."""
    return await call("component.set_property", componentId=component_id,
                      property=property_name, value=value)


@mcp.tool()
async def set_component_properties(component_id: str, values: dict[str, Any]) -> Any:
    """Atomically set multiple properties on an exact component. Invalid values roll
    the complete batch back; a successful batch becomes one editor Undo operation."""
    return await call("component.set_properties", componentId=component_id, values=values)


@mcp.tool()
async def invoke_component_action(component_id: str, action: str,
                                  arguments: dict[str, Any] | None = None) -> Any:
    """Invoke a discovered transient component action, such as AudioSource play,
    ParticleComponent emit_burst, Animator play/set_parameter, or set_script."""
    return await call("component.invoke", componentId=component_id, action=action,
                      arguments=arguments or {})

@mcp.tool()
async def set_component_enabled(object_id: str, component_type: str, enabled: bool) -> Any:
    """Enable or disable a component by type name (e.g. "SpriteRenderer", "RigidBody")."""
    return await call("component.set_enabled", id=object_id, type=component_type, value=enabled)


@mcp.tool()
async def get_component_enabled(object_id: str, component_type: str) -> Any:
    """Check whether a component is enabled, by type name."""
    return await call("component.get_enabled", id=object_id, type=component_type)


@mcp.tool()
async def set_sprite_color(object_id: str, r: float, g: float, b: float, a: float = 1.0) -> Any:
    """Set a SpriteRenderer's color. Channels are 0..1."""
    return await call("sprite_renderer.set_color", id=object_id, value=[r, g, b, a])


@mcp.tool()
async def set_sprite_visible(object_id: str, visible: bool) -> Any:
    """Show or hide a SpriteRenderer."""
    return await call("sprite_renderer.set_visible", id=object_id, value=visible)


@mcp.tool()
async def set_sprite_flip(object_id: str, flip_x: bool | None = None,
                          flip_y: bool | None = None) -> Any:
    """Flip a SpriteRenderer horizontally and/or vertically."""
    results = {}
    if flip_x is not None:
        results["flip_x"] = await call("sprite_renderer.set_flip_x", id=object_id, value=flip_x)
    if flip_y is not None:
        results["flip_y"] = await call("sprite_renderer.set_flip_y", id=object_id, value=flip_y)
    return results or {"error": "pass flip_x and/or flip_y"}


@mcp.tool()
async def get_velocity(object_id: str) -> Any:
    """Get a RigidBody's linear velocity."""
    return await call("rigidbody.get_velocity", id=object_id)


@mcp.tool()
async def set_velocity(object_id: str, x: float, y: float) -> Any:
    """Set a RigidBody's linear velocity."""
    return await call("rigidbody.set_velocity", id=object_id, x=x, y=y)


@mcp.tool()
async def set_body_type(object_id: str, body_type: str) -> Any:
    """Set a RigidBody's type: "Dynamic", "Kinematic" or "Static"."""
    return await call("rigidbody.set_body_type", id=object_id, value=body_type)


@mcp.tool()
async def apply_impulse(object_id: str, x: float, y: float) -> Any:
    """Apply an instantaneous impulse to a RigidBody. Only meaningful in play mode."""
    return await call("rigidbody.apply_impulse", id=object_id, x=x, y=y)


@mcp.tool()
async def apply_force(object_id: str, x: float, y: float) -> Any:
    """Apply a continuous force to a RigidBody. Only meaningful in play mode."""
    return await call("rigidbody.apply_force", id=object_id, x=x, y=y)


# --- Object lifecycle --------------------------------------------------------------

@mcp.tool()
async def create_object(sibling_id: str = "", name: str = "GameObject",
                        scene_id: str = "") -> Any:
    """Create an empty GameObject.

    Pass sibling_id to create it in the same scene as an existing object. Otherwise,
    pass scene_id; when neither is supplied, the only loaded scene is used. This lets
    an empty scene receive its first object.

    Not undoable in the editor -- MCP edits bypass the undo stack, like script edits.
    """
    return await call("object.instantiate", id=sibling_id, name=name, sceneId=scene_id)


@mcp.tool()
async def add_component(object_id: str, component_type: str) -> Any:
    """Attach a component to a GameObject by engine type name, e.g. "TextRenderer",
    "SpriteRenderer", "RigidBody", "BoxCollider", "Camera". Returns the component id."""
    return await call("object.add_component", id=object_id, type=component_type)


@mcp.tool()
async def set_text(object_id: str, text: str) -> Any:
    """Set a TextRenderer's string. The object needs a TextRenderer component first."""
    return await call("text_renderer.set_text", id=object_id, value=text)


@mcp.tool()
async def get_text(object_id: str) -> Any:
    """Read a TextRenderer's current string."""
    return await call("text_renderer.get_text", id=object_id)


@mcp.tool()
async def set_text_style(object_id: str, font_size: float | None = None,
                         font: str | None = None,
                         color: list[float] | None = None) -> Any:
    """Set a TextRenderer's font size, font name (e.g. "Nunito"), and/or RGBA color (0..1)."""
    results = {}
    if font_size is not None:
        results["font_size"] = await call("text_renderer.set_font_size", id=object_id, value=font_size)
    if font is not None:
        results["font"] = await call("text_renderer.set_font", id=object_id, value=font)
    if color is not None:
        results["color"] = await call("text_renderer.set_color", id=object_id, value=color)
    return results or {"error": "pass at least one of font_size, font, color"}


@mcp.tool()
async def duplicate_object(object_id: str) -> Any:
    """Duplicate a GameObject and its whole subtree. Not undoable."""
    return await call("object.duplicate", id=object_id)


@mcp.tool()
async def destroy_object(object_id: str, confirm: bool = False) -> Any:
    """Destroy a GameObject only after mandatory in-editor impact clarification.

    The answer bank reports descendants, component count, and external Joints that
    will cascade. The user can approve, deactivate instead, inspect, cancel, or use
    Other. `confirm` is retained for compatibility but cannot bypass clarification.
    """
    _ = confirm
    started = await call("object.destroy", id=object_id)
    impact = _editor_data(started)
    if impact.get("status") != "clarification_required":
        return started
    request_id = impact.get("requestId", "")
    answered_result = await _wait_for_clarification(request_id)
    answer = _editor_data(answered_result)
    selected = set(answer.get("selectedIds", []))
    if answer.get("status") == "answered" and "destroy" in selected:
        return await call("object.destroy", id=object_id, clarificationId=request_id)
    if answer.get("status") == "answered" and "deactivate" in selected:
        result = await call("object.set_active", id=object_id, value=False)
        return {"status": "alternative_applied", "choice": "deactivate",
                "result": result, "clarification": answer, "affected": impact.get("affected", [])}
    return {"status": "not_destroyed", "choice": next(iter(selected), "cancel"),
            "clarification": answer, "affected": impact.get("affected", []),
            "otherText": answer.get("otherText", "")}


# --- Engine mode -------------------------------------------------------------------

@mcp.tool()
async def get_engine_mode() -> Any:
    """Get the current mode: editor vs play, paused, and whether a transition is in flight."""
    return await call("engine.get_mode")


@mcp.tool()
async def enter_play_mode() -> Any:
    """Start play mode. Asynchronous -- poll get_engine_mode until isTransitioning is false.

    Play mode runs on a copy of the world; anything changed there is lost on exit.
    """
    return await call("engine.enter_play_mode")


@mcp.tool()
async def exit_play_mode() -> Any:
    """Stop play mode and return to the authored world, discarding runtime changes."""
    return await call("engine.exit_play_mode")


@mcp.tool()
async def pause_play_mode() -> Any:
    """Pause a running play-mode session."""
    return await call("engine.pause")


@mcp.tool()
async def resume_play_mode() -> Any:
    """Resume a paused play-mode session."""
    return await call("engine.resume")


@mcp.tool()
async def step_frame() -> Any:
    """Advance a paused play-mode session by exactly one frame."""
    return await call("engine.step_frame")


# --- Build -------------------------------------------------------------------------

@mcp.tool()
async def build_game(output_dir: str = "", run_after: bool = False) -> Any:
    """Build the game from Domain/sandbox/project.build. Asynchronous -- poll
    get_build_status. Requires the editor to not be in play mode."""
    return await call("build.trigger", outputDir=output_dir, runAfter=run_after)


@mcp.tool()
async def get_build_status() -> Any:
    """Check whether a build is running, and how the last one finished."""
    return await call("build.get_status")


# --- Inspection --------------------------------------------------------------------

@mcp.tool()
async def get_console_messages(message_type: str = "", limit: int = 100) -> Any:
    """Read recent engine/editor console messages, newest first. message_type may be
    comment, warning, or alert."""
    return await call("console.get_messages", type=message_type, limit=limit)


@mcp.tool()
async def capture_scene_view() -> Any:
    """Capture the visible Scene view as a PNG data URL for visual inspection."""
    return await call("scene.capture_view")


@mcp.tool()
async def run_last_build() -> Any:
    """Launch the most recent successful build of this session."""
    return await call("build.run")


if __name__ == "__main__":
    mcp.run()
