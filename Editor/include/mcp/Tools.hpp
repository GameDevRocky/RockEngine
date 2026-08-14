#pragma once

namespace mcp {

class McpDispatcher;

// Each group registers its tools into the dispatcher. Called once, from McpServer::Start.
void RegisterSceneTools(McpDispatcher& dispatcher);
void RegisterTransformTools(McpDispatcher& dispatcher);
void RegisterComponentTools(McpDispatcher& dispatcher);
void RegisterObjectTools(McpDispatcher& dispatcher);
void RegisterAssetTools(McpDispatcher& dispatcher);
void RegisterLifecycleTools(McpDispatcher& dispatcher);
void RegisterEngineModeTools(McpDispatcher& dispatcher);
void RegisterBuildTools(McpDispatcher& dispatcher);
void RegisterDebugTools(McpDispatcher& dispatcher);

} // namespace mcp
