#include "mcp/McpDispatcher.hpp"

#include "mcp/ToolSupport.hpp"

#include <exception>

namespace mcp {

namespace {

// JSON-RPC 2.0 reserved codes.
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInternalError  = -32603;

QJsonObject MakeResponse(const QJsonValue& id) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    return response;
}

QJsonObject MakeError(const QJsonValue& id, int code, const QString& message) {
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;

    QJsonObject response = MakeResponse(id);
    response["error"] = error;
    return response;
}

} // namespace

void McpDispatcher::RegisterTool(const std::string& method, McpToolHandler handler) {
    m_tools[method] = std::move(handler);
}

QJsonObject McpDispatcher::Dispatch(const QJsonObject& request) {
    const QJsonValue id = request.value("id");
    const QString method = request.value("method").toString();

    if (method.isEmpty())
        return MakeError(id, kInvalidRequest, "missing \"method\"");

    auto it = m_tools.find(method.toStdString());
    if (it == m_tools.end())
        return MakeError(id, kMethodNotFound, "unknown method: " + method);

    const QJsonObject params = request.value("params").toObject();

    McpResult result;
    try {
        result = it->second(params);
    } catch (const std::exception& e) {
        // Python exceptions land here too (py::error_already_set derives from
        // std::runtime_error), but PyApiCall translates those into a PythonError
        // result before they get this far -- deliberately keeping pybind11 out of
        // this file, since including it after Qt's headers breaks the build (Qt's
        // `slots` macro mangles CPython's object.h). This is the backstop.
        return MakeError(id, kInternalError, QString::fromUtf8(e.what()));
    } catch (...) {
        return MakeError(id, kInternalError, "unknown exception in tool handler");
    }

    if (!result.ok)
        return MakeError(id, result.errorCode, result.errorMessage);

    // Which world answered is attached centrally rather than per-tool, so it can
    // never be accidentally omitted from a tool that mutates state. Play mode runs
    // on a deep copy that is discarded on Stop, and a client that edits it without
    // knowing that would think its change had persisted.
    QJsonObject payload;
    payload["data"] = result.data;
    payload["worldMode"] = support::WorldMode();
    if (support::IsRuntimeWorld())
        payload["warning"] = "play-mode world: changes are discarded when play mode exits";

    QJsonObject response = MakeResponse(id);
    response["result"] = payload;
    return response;
}

} // namespace mcp
