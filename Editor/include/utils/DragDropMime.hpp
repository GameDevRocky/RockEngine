#pragma once

// Custom MIME type used when dragging a GameObject out of the scene Hierarchy
// (SceneTree). The payload is the GameObject's id (UTF-8). Inspector reference
// widgets accept this to let you drop a Hierarchy item onto a GameObject /
// component reference field. Assets from the Folder view instead arrive as the
// standard "text/uri-list" (file paths), so they need no custom type.
inline constexpr const char* kGameObjectMimeType = "application/x-rockengine-gameobject-id";
