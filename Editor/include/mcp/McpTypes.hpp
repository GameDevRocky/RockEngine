#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <functional>

// Shared vocabulary for the MCP bridge (see McpServer.hpp for the subsystem overview).
namespace mcp {

// Application-specific JSON-RPC error codes. The -32000..-32099 range is reserved
// for implementation-defined server errors by the spec, so these sit inside it and
// never collide with the standard parse/invalid-request codes the dispatcher emits.
enum ErrorCode {
    ObjectNotFound   = -32001,
    BuildInProgress  = -32002,
    PythonError      = -32003,
    WrongMode        = -32004,
};

struct McpResult {
    bool       ok = true;
    QJsonValue data;
    int        errorCode = 0;
    QString    errorMessage;

    static McpResult Ok(QJsonValue value = QJsonValue()) { return {true, std::move(value), 0, {}}; }
    static McpResult Error(int code, const QString& message) { return {false, {}, code, message}; }
};

using McpToolHandler = std::function<McpResult(const QJsonObject& params)>;

} // namespace mcp
