#pragma once

#include <QByteArray>
#include <QObject>

class QLocalSocket;

namespace mcp {

class McpDispatcher;

// One accepted client connection. Frames newline-delimited JSON: reads accumulate
// into a buffer that is split on '\n', because a socket read boundary has nothing to
// do with a message boundary -- one read may carry half a request or three of them.
class McpConnection : public QObject {
    Q_OBJECT
public:
    McpConnection(QLocalSocket* socket, McpDispatcher* dispatcher, QObject* parent = nullptr);

    void Close();

private:
    void OnReadyRead();
    void HandleLine(const QByteArray& line);

    QLocalSocket*  m_socket;
    McpDispatcher* m_dispatcher;
    QByteArray     m_buffer;
};

} // namespace mcp
