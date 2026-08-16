#pragma once

#include "mcp/McpDispatcher.hpp"

#include <QObject>
#include <QList>

class QLocalServer;

namespace mcp {

class McpConnection;

// Local IPC endpoint that lets an external process drive this editor: the engine half
// of the MCP bridge. An external stdio MCP server (tools/mcp-server/) connects here
// and forwards each tool call as one line of JSON-RPC 2.0.
//
// QLocalServer, not a TCP socket: a named pipe on Windows / Unix domain socket
// elsewhere is local by construction, so there is no listening port to secure.
//
// THREADING -- the reason this design needs no locks or job queue. QLocalServer's
// signals are delivered by the Qt event loop, which is the same main thread that runs
// Editor::FrameTick -> Engine::Update. Tool handlers therefore run *between* frames on
// the main thread and may touch Registry/Observable/Python directly, satisfying
// ROCK_ASSERT_MAIN_THREAD for free. Two consequences:
//   - No handler may call QApplication::processEvents() or exec(). That spins a nested
//     event loop in which frameSwapped still fires -> FrameTick -> Engine::Update ->
//     JobSystem::Pump, re-entering the pump from inside a socket callback. Same reason
//     this editor never exec()s a dialog (see LoadingOverlay.hpp).
//   - Long operations must not run inline. Game builds go through GameBuilder's
//     existing JobSystem split instead, and their tool returns as soon as it submits.
//     User questions become inline AI transcript cards and use later status-poll
//     requests for the same reason; never hold this callback open for UI input.
class McpServer : public QObject {
    Q_OBJECT
public:
    static McpServer* Get();

    // Call once from Editor::PostInit, after Engine::PostInit -- handlers reach for the
    // active container and the embedded interpreter's sys.path, both of which are only
    // set up by then.
    static void Install();
    // Call first in Editor::Shutdown, before any other teardown, so no in-flight request
    // can touch a half-destroyed engine.
    static void Shutdown();

    McpDispatcher& Dispatcher() { return m_dispatcher; }

private:
    McpServer() = default;

    void Start();
    void Stop();
    void OnNewConnection();

    QLocalServer*        m_server = nullptr;
    QList<McpConnection*> m_connections;
    McpDispatcher        m_dispatcher;
};

} // namespace mcp
