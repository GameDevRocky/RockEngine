#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QString>

namespace ai {

class AiAgentService : public QObject {
    Q_OBJECT
public:
    enum class Provider { OpenAI = 0, Claude = 1, Gemini = 2 };
    Q_ENUM(Provider)

    static AiAgentService* Get();

    void CheckAuthentication(Provider provider);
    void SignInWithAccount(Provider provider);
    void SignInWithApiKey(Provider provider, QByteArray apiKey);
    void SignOut(Provider provider);

    void SendMessage(Provider provider, const QString& model, const QString& message);
    void Cancel();
    void NewConversation(Provider provider);
    void Shutdown();

    bool IsBusy() const { return m_operation != Operation::None; }
    static QString ProviderName(Provider provider);
    static QString ApiKeyPortal(Provider provider);

signals:
    void AuthenticationChanged(ai::AiAgentService::Provider provider,
                               bool cliAvailable, bool authenticated,
                               const QString& detail);
    void AuthenticationOutput(ai::AiAgentService::Provider provider, const QString& text);
    void BusyChanged(bool busy);
    void ActivityChanged(const QString& text);
    void ResponseUpdated(ai::AiAgentService::Provider provider, const QString& text);
    void ResponseReady(ai::AiAgentService::Provider provider, const QString& text);
    void RequestFailed(ai::AiAgentService::Provider provider, const QString& error);

private:
    enum class Operation { None, AccountLogin, ApiKeyLogin, Logout, Chat };

    explicit AiAgentService(QObject* parent = nullptr);
    ~AiAgentService() override;

    QString FindCli(Provider provider) const;
    QString ProviderDataDirectory(Provider provider) const;
    QProcessEnvironment ProcessEnvironment(Provider provider) const;
    QString McpPythonPath() const;
    QString McpServerPath() const;
    QString McpConfigurationJson() const;
    QString ClaudeCredentialHelperCommand() const;
    bool EnsureGeminiSettings(QString* error = nullptr) const;
    QStringList CodexMcpArguments() const;
    QString SessionId(Provider provider) const;
    void SetSessionId(Provider provider, const QString& sessionId);
    QString InitialPrompt(const QString& userMessage) const;

    bool BeginProcess(Provider provider, Operation operation,
                      const QString& program, const QStringList& arguments,
                      QProcessEnvironment environment = {});
    void ReadStandardOutput();
    void ReadStandardError();
    void ParseChatLine(const QByteArray& line);
    void ParseCodexEvent(const QJsonObject& event);
    void SendCodexRequest(int id, const QString& method, const QJsonObject& params);
    void StartCodexThread();
    void StartCodexTurn(const QString& threadId);
    void ParseClaudeEvent(const QJsonObject& event);
    void ParseGeminiEvent(const QJsonObject& event);
    void PublishResponse(const QString& text, bool append);
    void ProcessFinished(int exitCode, QProcess::ExitStatus status);
    void ProcessError(QProcess::ProcessError error);
    void FinishOperation();
    void ClearProviderCredentialFiles(Provider provider);

    QProcess* m_process = nullptr;
    Provider m_activeProvider = Provider::OpenAI;
    Operation m_operation = Operation::None;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderr;
    QByteArray m_pendingStdin;
    QByteArray m_pendingCredential;
    QString m_responseText;
    QString m_codexPrompt;
    QString m_codexMessageItemId;
    bool m_codexAppServer = false;
    bool m_codexTurnCompleted = false;
    bool m_codexResumeAttempted = false;
    bool m_cancelled = false;
    bool m_processErrorReported = false;
};

} // namespace ai

Q_DECLARE_METATYPE(ai::AiAgentService::Provider)
