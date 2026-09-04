#include "mcp/McpServer.hpp"

#include "mcp/McpConnection.hpp"
#include "mcp/Tools.hpp"

#include "engine/debug/Console.hpp"

#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace mcp {

namespace {
// Versioned: a future protocol break can listen on .v2 and leave old bridges failing
// to connect rather than talking a dialect the editor no longer speaks.
constexpr const char* kServerName = "RockEngine.McpBridge.v1";
} // namespace

McpServer* McpServer::Get() {
    static McpServer* instance = new McpServer();
    return instance;

}

void McpServer::Install() { Get()->Start(); }
void McpServer::Shutdown() { Get()->Stop(); }

void McpServer::Start() {
    if (m_server) return;

    m_dispatcher.RegisterTool("ping", [](const QJsonObject&) {
        QJsonObject data;
        data["pong"] = true;
        return McpResult::Ok(data);
    });

    RegisterSceneTools(m_dispatcher);
    RegisterTransformTools(m_dispatcher);
    RegisterComponentTools(m_dispatcher);
    RegisterObjectTools(m_dispatcher);
    RegisterAssetTools(m_dispatcher);
    RegisterLifecycleTools(m_dispatcher);
    RegisterEngineModeTools(m_dispatcher);
    RegisterBuildTools(m_dispatcher);
    RegisterDebugTools(m_dispatcher);
    RegisterPropertyTools(m_dispatcher);
    RegisterClarificationTools(m_dispatcher);

    m_server = new QLocalServer(this);
    // An unclean exit leaves the socket file behind on Unix and the next listen()
    // fails with "address in use"; a no-op for Windows named pipes.
    QLocalServer::removeServer(kServerName);

    if (!m_server->listen(kServerName)) {
        Console::Warn("MCP bridge failed to listen: " + m_server->errorString().toStdString());
        delete m_server;
        m_server = nullptr;
        return;
    }

    connect(m_server, &QLocalServer::newConnection, this, &McpServer::OnNewConnection);
    Console::Comment(std::string("MCP bridge listening on ") + kServerName);
}

void McpServer::Stop() {
    if (!m_server) return;

    // Detach the list before closing anything: Close() can emit `disconnected`
    // synchronously, and that handler removes from m_connections -- mutating the
    // container mid-iteration.
    const QList<McpConnection*> closing = std::move(m_connections);
    m_connections.clear();
    for (McpConnection* connection : closing)
        connection->Close();
    qDeleteAll(closing);

    m_server->close();
    delete m_server;
    m_server = nullptr;
}

void McpServer::OnNewConnection() {
    while (QLocalSocket* socket = m_server->nextPendingConnection()) {
        auto* connection = new McpConnection(socket, &m_dispatcher, this);
        m_connections.append(connection);

        connect(socket, &QLocalSocket::disconnected, this, [this, connection]() {
            m_connections.removeOne(connection);
            connection->deleteLater();
        });
    }
}

} // namespace mcp
