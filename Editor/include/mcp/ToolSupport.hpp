#pragma once

#include "mcp/McpTypes.hpp"

#include <string>

class Container;
class GameObject;
class Registry;

namespace mcp {

// Helpers shared by every tool handler. All main-thread only, like the handlers themselves.
namespace support {

Container* ActiveContainer();
Registry*  ActiveRegistry();

// Ids are resolved fresh on every call, never cached. Entering play mode swaps in a
// deep copy of the world that preserves ids verbatim, and exiting deletes that whole
// registry -- so a stored GameObject* becomes a use-after-free the moment the user
// presses Stop, while a stored id keeps resolving against whichever world is live.
GameObject* FindGameObject(const std::string& id);

// Reads params["id"] and resolves it, or reports why it could not.
McpResult ResolveGameObject(const QJsonObject& params, GameObject** out);

// "Editor" | "Runtime" | "Paused" for the active container.
QString WorldMode();

// Set on results from tools that changed something, so a client is told in-band when
// its edit landed on the throwaway play-mode world instead of the authored one.
bool IsRuntimeWorld();

} // namespace support
} // namespace mcp
