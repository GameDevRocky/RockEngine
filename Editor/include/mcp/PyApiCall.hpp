#pragma once

#include "mcp/McpTypes.hpp"

#include <QString>
#include <string>
#include <vector>

namespace mcp {

// Reads and writes component state through Domain/lib/api -- the same Python handler
// classes user scripts use -- rather than reaching for the raw rock_engine bindings or
// re-implementing the setters in C++. Going through the handlers means id resolution,
// type coercion and Vector2 packing stay in one place, and an MCP edit behaves exactly
// like the equivalent line in a user script.
//
// Consequence worth knowing: like script-driven edits, these bypass UndoSystem and do
// not appear in Ctrl+Z. That is the existing convention (see GizmoUndoBridge.hpp) --
// undo is recorded at editor call sites, not inside engine mutators.
//
// Every entry point acquires the GIL itself and translates a Python exception into a
// PythonError result. The interpreter is started in Engine::Init with the GIL released,
// so nothing here may assume it is already held.
namespace pyapi {

// A component handler class: where to import it from, what it is called, and the
// engine-side Component::GetTypeName() it corresponds to.
struct HandlerClass {
    const char* module;
    const char* name;
    const char* typeName;
};

inline constexpr HandlerClass kTransform{
    "Domain.lib.api.components.transform_handler", "Transform", "Transform"};
inline constexpr HandlerClass kSpriteRenderer{
    "Domain.lib.api.components.sprite_renderer_handler", "SpriteRenderer", "SpriteRenderer"};
// Class name is `Rigidbody`; the engine type it wraps is `RigidBody`.
inline constexpr HandlerClass kRigidBody{
    "Domain.lib.api.components.rigidbody_handler", "Rigidbody", "RigidBody"};
inline constexpr HandlerClass kTextRenderer{
    "Domain.lib.api.components.text_renderer_handler", "TextRenderer", "TextRenderer"};
inline constexpr HandlerClass kAudioSource{
    "Domain.lib.api.components.audio_source_handler", "AudioSource", "AudioSource"};

// Not a component. Constructed the same way (one id), so the same property helpers
// work on it -- which is how name/tag/active are reached. Deliberately excluded from
// the component type lookup below.
inline constexpr HandlerClass kGameObject{
    "Domain.lib.api.core.gameobject_handler", "GameObject", "GameObject"};

// Asset wrappers. Also id-constructed, but they appear on the VALUE side of a
// property (SpriteRenderer.sprite wants a Sprite, not an id string).
inline constexpr HandlerClass kSpriteAsset{
    "Domain.lib.api.rendering.sprite_handler", "Sprite", "Sprite"};
inline constexpr HandlerClass kAudioClipAsset{
    "Domain.lib.api.audio.audio_clip_handler", "AudioClip", "AudioClip"};

// Resolve a handler by engine component type name, for tools that work against any
// component type (e.g. enabling/disabling one). Null when the type has no handler.
const HandlerClass* HandlerForType(const QString& typeName);

// handler(objectId).<property>
McpResult GetProperty(const HandlerClass& handler, const std::string& objectId, const char* property);
McpResult SetProperty(const HandlerClass& handler, const std::string& objectId, const char* property,
                      const QJsonValue& value);

// handler(objectId).<method>(*args)
McpResult CallMethod(const HandlerClass& handler, const std::string& objectId, const char* method,
                     const std::vector<QJsonValue>& args = {});

// handler(objectId).<property> = assetClass(assetId)
//
// Separate from SetProperty because these setters read `value.id` off a wrapper object;
// handing them a bare id string raises AttributeError. An empty assetId assigns None.
McpResult SetAssetProperty(const HandlerClass& handler, const std::string& objectId,
                           const char* property, const HandlerClass& assetClass,
                           const std::string& assetId);

} // namespace pyapi
} // namespace mcp
