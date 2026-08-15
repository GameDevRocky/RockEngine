#pragma once

// Custom MIME type used when dragging a GameObject out of the scene Hierarchy
// (SceneTree). The payload is the GameObject's id (UTF-8). Inspector reference
// widgets accept this to let you drop a Hierarchy item onto a GameObject /
// component reference field. Assets from the Folder view instead arrive as the
// standard "text/uri-list" (file paths), so they need no custom type.
inline constexpr const char* kGameObjectMimeType = "application/x-rockengine-gameobject-id";

// Several GameObjects at once: newline-separated ids (UTF-8). Emitted by a
// multi-selection drag out of the scene Hierarchy. A drag that carries this
// ALSO carries kGameObjectMimeType holding the first id, so single-value
// targets (Inspector reference fields) keep working untouched — only targets
// that can take a list (e.g. the AI assistant's context area) read this one.
inline constexpr const char* kGameObjectListMimeType = "application/x-rockengine-gameobject-id-list";

// Custom MIME type used when dragging a live component out of its Inspector
// section. The payload is the component id (UTF-8). Unlike a file attachment,
// consumers resolve this id at drop/send time so they see the current in-memory
// component state and its owning GameObject.
inline constexpr const char* kComponentMimeType = "application/x-rockengine-component-id";

// Custom MIME type used when dragging a Sprite out of the Folder view's texture
// hover column. The payload is the Sprite's id (UTF-8). A Sprite is a sub-asset of
// a texture with no file of its own, so it can't ride "text/uri-list" like other
// assets — SPRITE reference fields accept this type instead (see RefDropFilter).
inline constexpr const char* kSpriteMimeType = "application/x-rockengine-sprite-id";

// Several sprites at once: newline-separated ids (UTF-8). Emitted by a
// multi-selection drag out of the asset picker, and by any future multi-select
// sprite source. A drag that carries this ALSO carries kSpriteMimeType holding
// the first id, so single-value fields keep working untouched — only targets
// that can take a list (the Animator's frame strip) read this one.
inline constexpr const char* kSpriteListMimeType = "application/x-rockengine-sprite-id-list";
