#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <functional>

// Shared vocabulary for the MCP bridge (see McpServer.hpp for the subsystem overview).
namespace mcp {

// InvalidParams is the standard JSON-RPC code. The remaining application-specific
// errors use the spec-reserved -32000..-32099 implementation range.
enum ErrorCode {
    InvalidParams    = -32602,
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
