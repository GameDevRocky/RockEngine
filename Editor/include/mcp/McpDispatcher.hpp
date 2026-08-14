#pragma once

#include "mcp/McpTypes.hpp"

#include <string>
#include <unordered_map>

namespace mcp {

// Method-name -> handler registry, plus the JSON-RPC envelope handling around a call.
//
// Handlers run synchronously on the Qt main thread (see McpServer.hpp), so they may
// touch Registry/Observable/Python freely. They must never throw past Dispatch --
// Dispatch catches everything, including py::error_already_set, so one bad tool call
// degrades to an error response instead of taking down the editor.
class McpDispatcher {
public:
    void RegisterTool(const std::string& method, McpToolHandler handler);

    // Takes a parsed JSON-RPC request object, returns the response object to write back.
    QJsonObject Dispatch(const QJsonObject& request);

private:
    std::unordered_map<std::string, McpToolHandler> m_tools;
};

} // namespace mcp
