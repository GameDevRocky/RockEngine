#include "mcp/McpConnection.hpp"

#include "mcp/McpDispatcher.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

namespace mcp {

namespace {
constexpr int kParseError = -32700;

// A single request should never be anywhere near this large; the cap stops a client
// that never sends a newline from growing the buffer without bound.
constexpr int kMaxLineBytes = 4 * 1024 * 1024;
} // namespace

McpConnection::McpConnection(QLocalSocket* socket, McpDispatcher* dispatcher, QObject* parent)
    : QObject(parent), m_socket(socket), m_dispatcher(dispatcher) {
    m_socket->setParent(this);
    connect(m_socket, &QLocalSocket::readyRead, this, &McpConnection::OnReadyRead);
}

void McpConnection::Close() {
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->disconnectFromServer();
        m_socket->close();
    }
}

void McpConnection::OnReadyRead() {
    m_buffer.append(m_socket->readAll());

    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (!line.trimmed().isEmpty())
            HandleLine(line);
    }

    if (m_buffer.size() > kMaxLineBytes) {
        m_buffer.clear();
        Close();
    }
}

void McpConnection::HandleLine(const QByteArray& line) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

    QJsonObject response;
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        // Per JSON-RPC, a request whose id could not be parsed is answered with a null id.
        QJsonObject error;
        error["code"] = kParseError;
        error["message"] = "invalid JSON: " + parseError.errorString();
        response["jsonrpc"] = "2.0";
        response["id"] = QJsonValue();
        response["error"] = error;
    } else {
        response = m_dispatcher->Dispatch(doc.object());
    }

    m_socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
    m_socket->flush();
}

} // namespace mcp
