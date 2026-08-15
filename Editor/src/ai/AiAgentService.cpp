#include "ai/AiAgentService.hpp"

#include "ai/SecureCredentialStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace ai {
namespace {

constexpr const char* kClaudeCredentialAccount = "claude-api-key";
constexpr const char* kGeminiCredentialAccount = "gemini-api-key";

QString ProviderKey(AiAgentService::Provider provider) {
    switch (provider) {
    case AiAgentService::Provider::OpenAI: return QStringLiteral("openai");
    case AiAgentService::Provider::Claude: return QStringLiteral("claude");
    case AiAgentService::Provider::Gemini: return QStringLiteral("gemini");
    }
    return {};
}

QString ProjectRoot() {
    return QDir::cleanPath(QString::fromUtf8(PROJECT_ROOT));
}

QString TomlLiteral(QString value) {
    value.replace('\\', '/');
    value.replace(QStringLiteral("'"), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

QString ShellQuote(QString value) {
#ifdef Q_OS_WIN
    value.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(value);
#else
    value.replace(QStringLiteral("'"), QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(value);
#endif
}

QString EventToolName(const QJsonObject& item) {
    QString name = item.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) name = item.value(QStringLiteral("tool")).toString();
    if (name.isEmpty()) name = item.value(QStringLiteral("server")).toString();
    return name;
}

QString FriendlyToolName(QString name) {
    name.replace(QStringLiteral("mcp__rockengine__"), QString());
    name.replace('_', ' ');
    return name.trimmed();
}

} // namespace

AiAgentService* AiAgentService::Get() {
    static AiAgentService* instance = new AiAgentService();
    return instance;
}

AiAgentService::AiAgentService(QObject* parent) : QObject(parent) {
    qRegisterMetaType<Provider>();
    // Import the trusted credential module and stage the helper before any
    // agent can edit workspace files. Python caches the imported module, while
    // Claude receives a read-only copy outside its writable workspace.
    QString ignored;
    SecureCredentialStore::IsAvailable(&ignored);
    ClaudeCredentialHelperCommand();
}

AiAgentService::~AiAgentService() {
    Shutdown();
}

QString AiAgentService::ProviderName(Provider provider) {
    switch (provider) {
    case Provider::OpenAI: return QStringLiteral("OpenAI");
    case Provider::Claude: return QStringLiteral("Claude");
    case Provider::Gemini: return QStringLiteral("Gemini");
    }
    return {};
}

QString AiAgentService::ApiKeyPortal(Provider provider) {
    switch (provider) {
    case Provider::OpenAI:
        return QStringLiteral("https://platform.openai.com/api-keys");
    case Provider::Claude:
        return QStringLiteral("https://console.anthropic.com/settings/keys");
    case Provider::Gemini:
        return QStringLiteral("https://aistudio.google.com/app/apikey");
    }
    return {};
}

QString AiAgentService::FindCli(Provider provider) const {
    const char* overrideName = provider == Provider::OpenAI
        ? "ROCKENGINE_CODEX_CLI"
        : (provider == Provider::Claude ? "ROCKENGINE_CLAUDE_CLI"
                                        : "ROCKENGINE_GEMINI_CLI");
    const QString overridePath = qEnvironmentVariable(overrideName);
    if (!overridePath.isEmpty() && QFileInfo::exists(overridePath))
        return QDir::cleanPath(overridePath);

    const QString executableName = provider == Provider::OpenAI
        ? QStringLiteral("codex")
        : (provider == Provider::Claude ? QStringLiteral("claude")
                                        : QStringLiteral("gemini"));
    const QString onPath = QStandardPaths::findExecutable(executableName);
    if (!onPath.isEmpty()) return onPath;

    const QString home = QDir::homePath();
    const QStringList directCandidates = provider == Provider::Claude
        ? QStringList{
              home + QStringLiteral("/.local/bin/claude"),
              home + QStringLiteral("/.local/bin/claude.exe")}
        : QStringList{};
    for (const QString& candidate : directCandidates) {
        if (QFileInfo::exists(candidate)) return QDir::cleanPath(candidate);
    }

#ifdef Q_OS_WIN
    if (provider == Provider::OpenAI) {
        QDir extensions(home + QStringLiteral("/.vscode/extensions"));
        const QStringList versions = extensions.entryList(
            {QStringLiteral("openai.chatgpt-*-win32-x64")}, QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::Reversed);
        for (const QString& version : versions) {
            const QString candidate = extensions.filePath(
                version + QStringLiteral("/bin/windows-x86_64/codex.exe"));
            if (QFileInfo::exists(candidate)) return QDir::cleanPath(candidate);
        }
    }
#endif
    return {};
}

QString AiAgentService::ProviderDataDirectory(Provider provider) const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.rockengine");
    const QString path = QDir(base).filePath(
        QStringLiteral("ai/%1").arg(ProviderKey(provider)));
    QDir().mkpath(path);
    return QDir::cleanPath(path);
}

QProcessEnvironment AiAgentService::ProcessEnvironment(Provider provider) const {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // Never accidentally inherit a shell-level key and silently select a
    // different billing identity than the one shown in the widget.
    environment.remove(QStringLiteral("OPENAI_API_KEY"));
    environment.remove(QStringLiteral("ANTHROPIC_API_KEY"));
    environment.remove(QStringLiteral("ANTHROPIC_AUTH_TOKEN"));
    environment.remove(QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN"));
    environment.remove(QStringLiteral("GOOGLE_API_KEY"));
    environment.remove(QStringLiteral("GEMINI_API_KEY"));

    if (provider == Provider::OpenAI) {
        environment.insert(QStringLiteral("CODEX_HOME"), ProviderDataDirectory(provider));
    } else if (provider == Provider::Claude) {
        environment.insert(QStringLiteral("CLAUDE_CONFIG_DIR"), ProviderDataDirectory(provider));
    } else {
        environment.insert(QStringLiteral("GEMINI_CLI_HOME"), ProviderDataDirectory(provider));
        environment.insert(QStringLiteral("GEMINI_CLI_TRUST_WORKSPACE"), QStringLiteral("true"));
        QByteArray key = SecureCredentialStore::Load(
            QString::fromLatin1(kGeminiCredentialAccount));
        if (!key.isEmpty()) {
            environment.insert(QStringLiteral("GEMINI_API_KEY"), QString::fromUtf8(key));
        }
        key.fill('\0');
    }
    return environment;
}

QString AiAgentService::McpPythonPath() const {
#ifdef Q_OS_WIN
    return QDir(ProjectRoot()).filePath(
        QStringLiteral("tools/mcp-server/.venv/Scripts/python.exe"));
#else
    return QDir(ProjectRoot()).filePath(
        QStringLiteral("tools/mcp-server/.venv/bin/python"));
#endif
}

QString AiAgentService::McpServerPath() const {
    return QDir(ProjectRoot()).filePath(QStringLiteral("tools/mcp-server/server.py"));
}

QString AiAgentService::McpConfigurationJson() const {
    QJsonObject server;
    server.insert(QStringLiteral("command"), QDir::toNativeSeparators(McpPythonPath()));
    server.insert(QStringLiteral("args"), QJsonArray{QDir::toNativeSeparators(McpServerPath())});

    QJsonObject servers;
    servers.insert(QStringLiteral("rockengine"), server);
    QJsonObject root;
    root.insert(QStringLiteral("mcpServers"), servers);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool AiAgentService::EnsureGeminiSettings(QString* error) const {
    QJsonObject server;
    server.insert(QStringLiteral("command"), QDir::toNativeSeparators(McpPythonPath()));
    server.insert(QStringLiteral("args"), QJsonArray{
        QDir::toNativeSeparators(McpServerPath())});
    server.insert(QStringLiteral("cwd"), QDir::toNativeSeparators(ProjectRoot()));
    server.insert(QStringLiteral("trust"), true);

    QJsonObject servers;
    servers.insert(QStringLiteral("rockengine"), server);
    QJsonObject root;
    root.insert(QStringLiteral("mcpServers"), servers);

    const QString settingsDirectory = QDir(ProviderDataDirectory(Provider::Gemini))
        .filePath(QStringLiteral(".gemini"));
    if (!QDir().mkpath(settingsDirectory)) {
        if (error) *error = QStringLiteral("Could not create Gemini's isolated settings directory");
        return false;
    }

    QFile file(QDir(settingsDirectory).filePath(QStringLiteral("settings.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString AiAgentService::ClaudeCredentialHelperCommand() const {
    QString python = McpPythonPath();
    if (!QFileInfo::exists(python)) {
        python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        if (python.isEmpty())
            python = QStandardPaths::findExecutable(QStringLiteral("python"));
    }
    const QString sourceHelper = QDir(ProjectRoot()).filePath(
        QStringLiteral("tools/ai/credential_store.py"));
    const QString helper = QDir(ProviderDataDirectory(Provider::Claude)).filePath(
        QStringLiteral("credential_store_v1.py"));
    if (!QFileInfo::exists(helper)) {
        QFile::copy(sourceHelper, helper);
        QFile::setPermissions(helper, QFileDevice::ReadOwner | QFileDevice::ReadUser);
    }
    if (!QFileInfo::exists(python) || !QFileInfo::exists(helper)) return {};
    return QStringLiteral("%1 %2 get %3")
        .arg(ShellQuote(QDir::toNativeSeparators(python)),
             ShellQuote(QDir::toNativeSeparators(helper)),
             QString::fromLatin1(kClaudeCredentialAccount));
}

QStringList AiAgentService::CodexMcpArguments() const {
    if (!QFileInfo::exists(McpPythonPath()) || !QFileInfo::exists(McpServerPath())) return {};
    return {
        QStringLiteral("-c"),
        QStringLiteral("mcp_servers.rockengine.command=%1").arg(TomlLiteral(McpPythonPath())),
        QStringLiteral("-c"),
        QStringLiteral("mcp_servers.rockengine.args=[%1]").arg(TomlLiteral(McpServerPath())),
        QStringLiteral("-c"),
        QStringLiteral("mcp_servers.rockengine.cwd=%1").arg(TomlLiteral(ProjectRoot())),
        QStringLiteral("-c"),
        QStringLiteral("mcp_servers.rockengine.default_tools_approval_mode='approve'")};
}

QString AiAgentService::SessionId(Provider provider) const {
    QSettings settings;
    return settings.value(
        QStringLiteral("ai/%1/sessionId").arg(ProviderKey(provider))).toString();
}

void AiAgentService::SetSessionId(Provider provider, const QString& sessionId) {
    QSettings settings;
    const QString key = QStringLiteral("ai/%1/sessionId").arg(ProviderKey(provider));
    if (sessionId.isEmpty()) settings.remove(key);
    else settings.setValue(key, sessionId);
}

QString AiAgentService::InitialPrompt(const QString& userMessage) const {
    return QStringLiteral(
        "You are the AI assistant embedded in the running RockEngine Editor. "
        "Use the rockengine MCP tools for live scene, object, component, asset, play-mode, "
        "and build operations. The editor is already running; never launch another editor or "
        "player process. Use repository tools for source-code changes and follow AGENTS.md and "
        "CLAUDE.md. Clearly distinguish persistent editor-world edits from temporary play-mode "
        "edits. Do not destroy objects, overwrite scenes, change engine mode, build, or run a "
        "game unless the user explicitly asks for that action.\n\nUser request:\n%1").arg(userMessage);
}

void AiAgentService::CheckAuthentication(Provider provider) {
    const QString cli = FindCli(provider);
    if (cli.isEmpty()) {
        emit AuthenticationChanged(provider, false, false,
            QStringLiteral("%1 CLI was not found").arg(
                provider == Provider::OpenAI ? QStringLiteral("Codex")
                    : (provider == Provider::Claude ? QStringLiteral("Claude Code")
                                                    : QStringLiteral("Gemini"))));
        return;
    }

    if (provider == Provider::Claude || provider == Provider::Gemini) {
        const char* account = provider == Provider::Claude
            ? kClaudeCredentialAccount : kGeminiCredentialAccount;
        QString vaultError;
        QByteArray key = SecureCredentialStore::Load(
            QString::fromLatin1(account), &vaultError);
        const bool authenticated = !key.isEmpty();
        key.fill('\0');
        const QString providerName = ProviderName(provider);
        emit AuthenticationChanged(
            provider, true, authenticated,
            authenticated ? QStringLiteral("%1 API key in the OS credential vault")
                                .arg(providerName)
                          : (vaultError.isEmpty() ? QStringLiteral("%1 API key required")
                                                       .arg(providerName)
                                                  : vaultError));
        return;
    }

    auto* check = new QProcess(this);
    check->setProgram(cli);
    check->setArguments({
        QStringLiteral("-c"),
        QStringLiteral("cli_auth_credentials_store=\"keyring\""),
        QStringLiteral("login"), QStringLiteral("status")});
    check->setProcessEnvironment(ProcessEnvironment(provider));
    check->setWorkingDirectory(ProjectRoot());
    connect(check, &QProcess::finished, this,
            [this, check, provider](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(
            check->readAllStandardOutput() + check->readAllStandardError()).trimmed();
        const bool authenticated = status == QProcess::NormalExit && exitCode == 0;
        emit AuthenticationChanged(
            provider, true, authenticated,
            output.isEmpty()
                ? (authenticated ? QStringLiteral("Signed in") : QStringLiteral("Sign in required"))
                : output);
        check->deleteLater();
    });
    connect(check, &QProcess::errorOccurred, this,
            [this, check, provider](QProcess::ProcessError) {
        emit AuthenticationChanged(provider, false, false, check->errorString());
        if (check->state() == QProcess::NotRunning) check->deleteLater();
    });
    check->start();
}

bool AiAgentService::BeginProcess(Provider provider, Operation operation,
                                  const QString& program, const QStringList& arguments,
                                  QProcessEnvironment environment) {
    if (IsBusy()) {
        emit RequestFailed(provider, QStringLiteral("Another AI operation is already running"));
        return false;
    }
    if (program.isEmpty()) {
        emit RequestFailed(provider, QStringLiteral("Provider CLI is not installed or configured"));
        return false;
    }

    m_activeProvider = provider;
    m_operation = operation;
    m_stdoutBuffer.clear();
    m_stderr.clear();
    m_responseText.clear();
    m_cancelled = false;
    m_processErrorReported = false;

    m_process = new QProcess(this);
    m_process->setProgram(program);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(ProjectRoot());
    if (environment.isEmpty()) environment = ProcessEnvironment(provider);
    m_process->setProcessEnvironment(environment);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &AiAgentService::ReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &AiAgentService::ReadStandardError);
    connect(m_process, &QProcess::finished,
            this, &AiAgentService::ProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &AiAgentService::ProcessError);
    connect(m_process, &QProcess::started, this, [this]() {
        if (!m_pendingStdin.isEmpty()) {
            m_process->write(m_pendingStdin);
            m_pendingStdin.fill('\0');
            m_pendingStdin.clear();
        }
        m_process->closeWriteChannel();
    });

    emit BusyChanged(true);
    m_process->start();
    return true;
}

void AiAgentService::SignInWithAccount(Provider provider) {
    if (provider != Provider::OpenAI) {
        emit RequestFailed(provider,
            provider == Provider::Claude
                ? QStringLiteral("Anthropic requires third-party integrations to use a Console API key")
                : QStringLiteral("Gemini account login requires its interactive terminal; use an AI Studio API key in RockEngine"));
        return;
    }
    emit AuthenticationOutput(provider,
        QStringLiteral("Opening the secure ChatGPT sign-in flow in your browser…"));
    BeginProcess(provider, Operation::AccountLogin, FindCli(provider),
                 {QStringLiteral("-c"),
                  QStringLiteral("cli_auth_credentials_store=\"keyring\""),
                  QStringLiteral("login")},
                 ProcessEnvironment(provider));
}

void AiAgentService::SignInWithApiKey(Provider provider, QByteArray apiKey) {
    apiKey = apiKey.trimmed();
    if (apiKey.isEmpty()) {
        emit RequestFailed(provider, QStringLiteral("Enter an API key first"));
        return;
    }

    if (provider == Provider::Gemini) {
        // An invalid replacement must not silently fall back to the old key.
        SecureCredentialStore::Remove(QString::fromLatin1(kGeminiCredentialAccount));
        QProcessEnvironment environment = ProcessEnvironment(provider);
        environment.insert(QStringLiteral("GEMINI_API_KEY"), QString::fromUtf8(apiKey));
        m_pendingCredential = apiKey;
        apiKey.fill('\0');
        emit AuthenticationOutput(provider, QStringLiteral("Validating the Gemini API key…"));
        if (!BeginProcess(provider, Operation::ApiKeyLogin, FindCli(provider),
                          {QStringLiteral("--output-format"), QStringLiteral("json"),
                           QStringLiteral("--prompt"), QStringLiteral("Reply only with OK.")},
                          environment)) {
            m_pendingCredential.fill('\0');
            m_pendingCredential.clear();
        }
        return;
    }

    if (provider == Provider::Claude) {
        QString error;
        const bool stored = SecureCredentialStore::Store(
            QString::fromLatin1(kClaudeCredentialAccount), apiKey, &error);
        apiKey.fill('\0');
        if (!stored) {
            emit RequestFailed(provider,
                QStringLiteral("Could not store the Claude API key securely: %1").arg(error));
            return;
        }
        emit AuthenticationOutput(provider,
            QStringLiteral("Claude API key stored in the OS credential vault."));
        CheckAuthentication(provider);
        return;
    }

    m_pendingStdin = apiKey;
    m_pendingStdin.append('\n');
    apiKey.fill('\0');
    emit AuthenticationOutput(provider,
        QStringLiteral("Passing the key to Codex over stdin; RockEngine does not store it."));
    if (!BeginProcess(provider, Operation::ApiKeyLogin, FindCli(provider),
                      {QStringLiteral("-c"),
                       QStringLiteral("cli_auth_credentials_store=\"keyring\""),
                       QStringLiteral("login"), QStringLiteral("--with-api-key")},
                      ProcessEnvironment(provider))) {
        m_pendingStdin.fill('\0');
        m_pendingStdin.clear();
    }
}

void AiAgentService::ClearProviderCredentialFiles(Provider provider) {
    const QString directory = ProviderDataDirectory(provider);
    if (provider == Provider::OpenAI) {
        QFile::remove(QDir(directory).filePath(QStringLiteral("auth.json")));
    } else if (provider == Provider::Claude) {
        QFile::remove(QDir(directory).filePath(QStringLiteral(".credentials.json")));
    } else {
        const QDir geminiDirectory(QDir(directory).filePath(QStringLiteral(".gemini")));
        QFile::remove(geminiDirectory.filePath(QStringLiteral("oauth_creds.json")));
        QFile::remove(geminiDirectory.filePath(QStringLiteral("google_accounts.json")));
    }
}

void AiAgentService::SignOut(Provider provider) {
    NewConversation(provider);
    if (provider == Provider::Claude || provider == Provider::Gemini) {
        const char* account = provider == Provider::Claude
            ? kClaudeCredentialAccount : kGeminiCredentialAccount;
        QString error;
        const bool removed = SecureCredentialStore::Remove(
            QString::fromLatin1(account), &error);
        ClearProviderCredentialFiles(provider);
        if (!removed) {
            emit RequestFailed(provider,
                QStringLiteral("Could not erase the %1 credential: %2")
                    .arg(ProviderName(provider), error));
            return;
        }
        emit AuthenticationOutput(provider,
            QStringLiteral("%1 credentials erased.").arg(ProviderName(provider)));
        CheckAuthentication(provider);
        return;
    }

    emit AuthenticationOutput(provider, QStringLiteral("Erasing OpenAI credentials…"));
    if (!BeginProcess(provider, Operation::Logout, FindCli(provider),
                      {QStringLiteral("-c"),
                       QStringLiteral("cli_auth_credentials_store=\"keyring\""),
                       QStringLiteral("logout")},
                      ProcessEnvironment(provider))) {
        ClearProviderCredentialFiles(provider);
        CheckAuthentication(provider);
    }
}

void AiAgentService::SendMessage(Provider provider, const QString& message) {
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) return;

    const QString cli = FindCli(provider);
    if (cli.isEmpty()) {
        emit RequestFailed(provider, QStringLiteral("Provider CLI is not installed or configured"));
        return;
    }

    const QString sessionId = SessionId(provider);
    const QString prompt = sessionId.isEmpty() ? InitialPrompt(trimmed) : trimmed;
    m_pendingStdin = prompt.toUtf8();

    QStringList arguments;
    QProcessEnvironment environment = ProcessEnvironment(provider);
    if (provider == Provider::OpenAI) {
        arguments = CodexMcpArguments();
        arguments << QStringLiteral("-c")
                  << QStringLiteral("cli_auth_credentials_store=\"keyring\"")
                  << QStringLiteral("--ask-for-approval") << QStringLiteral("never")
                  << QStringLiteral("--sandbox") << QStringLiteral("workspace-write")
                  << QStringLiteral("--cd") << ProjectRoot()
                  << QStringLiteral("exec") << QStringLiteral("--json")
                  << QStringLiteral("--color") << QStringLiteral("never");
        if (!sessionId.isEmpty())
            arguments << QStringLiteral("resume") << sessionId;
        arguments << QStringLiteral("-");
    } else if (provider == Provider::Claude) {
        QString vaultError;
        QByteArray key = SecureCredentialStore::Load(
            QString::fromLatin1(kClaudeCredentialAccount), &vaultError);
        if (key.isEmpty()) {
            m_pendingStdin.clear();
            emit AuthenticationChanged(provider, true, false,
                vaultError.isEmpty() ? QStringLiteral("Claude API key required") : vaultError);
            emit RequestFailed(provider, QStringLiteral("Sign in with a Claude API key first"));
            return;
        }
        key.fill('\0');

        const QString helperCommand = ClaudeCredentialHelperCommand();
        if (helperCommand.isEmpty()) {
            m_pendingStdin.clear();
            emit RequestFailed(provider,
                QStringLiteral("Claude needs a Python executable for its secure credential helper"));
            return;
        }
        QJsonObject settings;
        settings.insert(QStringLiteral("apiKeyHelper"), helperCommand);
        const QString inlineSettings = QString::fromUtf8(
            QJsonDocument(settings).toJson(QJsonDocument::Compact));

        arguments << QStringLiteral("--bare")
                  << QStringLiteral("-p")
                  << QStringLiteral("--output-format") << QStringLiteral("stream-json")
                  << QStringLiteral("--verbose")
                  << QStringLiteral("--settings") << inlineSettings
                  << QStringLiteral("--permission-mode") << QStringLiteral("acceptEdits")
                  << QStringLiteral("--tools")
                  << QStringLiteral("Read,Glob,Grep,Edit,Write")
                  << QStringLiteral("--allowedTools")
                  << QStringLiteral("Read,Glob,Grep,Edit,Write,mcp__rockengine__*");
        if (QFileInfo::exists(McpPythonPath()) && QFileInfo::exists(McpServerPath()))
            arguments << QStringLiteral("--mcp-config") << McpConfigurationJson();
        if (!sessionId.isEmpty())
            arguments << QStringLiteral("--resume") << sessionId;
    } else {
        QString vaultError;
        QByteArray key = SecureCredentialStore::Load(
            QString::fromLatin1(kGeminiCredentialAccount), &vaultError);
        if (key.isEmpty()) {
            m_pendingStdin.clear();
            emit AuthenticationChanged(provider, true, false,
                vaultError.isEmpty() ? QStringLiteral("Gemini API key required") : vaultError);
            emit RequestFailed(provider, QStringLiteral("Sign in with a Gemini API key first"));
            return;
        }
        key.fill('\0');

        QString settingsError;
        if (!EnsureGeminiSettings(&settingsError)) {
            m_pendingStdin.clear();
            emit RequestFailed(provider,
                QStringLiteral("Could not configure Gemini MCP access: %1").arg(settingsError));
            return;
        }

        arguments << QStringLiteral("--output-format") << QStringLiteral("stream-json")
                  << QStringLiteral("--approval-mode") << QStringLiteral("yolo")
                  << QStringLiteral("--allowed-mcp-server-names")
                  << QStringLiteral("rockengine");
        if (!sessionId.isEmpty())
            arguments << QStringLiteral("--resume") << sessionId;
        arguments << QStringLiteral("--prompt") << prompt;
        m_pendingStdin.clear();
    }

    emit ActivityChanged(QStringLiteral("%1 is thinking…").arg(ProviderName(provider)));
    if (!BeginProcess(provider, Operation::Chat, cli, arguments, environment))
        m_pendingStdin.clear();
}

void AiAgentService::ReadStandardOutput() {
    if (!m_process) return;
    const QByteArray bytes = m_process->readAllStandardOutput();
    if (m_operation == Operation::Chat) {
        m_stdoutBuffer.append(bytes);
        while (true) {
            const qsizetype newline = m_stdoutBuffer.indexOf('\n');
            if (newline < 0) break;
            const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
            m_stdoutBuffer.remove(0, newline + 1);
            if (!line.isEmpty()) ParseChatLine(line);
        }
    } else {
        if (m_operation == Operation::ApiKeyLogin &&
            m_activeProvider == Provider::Gemini) {
            m_stdoutBuffer.append(bytes);
        }
        const QString text = QString::fromUtf8(bytes).trimmed();
        if (!text.isEmpty()) emit AuthenticationOutput(m_activeProvider, text);
    }
}

void AiAgentService::ReadStandardError() {
    if (!m_process) return;
    const QByteArray bytes = m_process->readAllStandardError();
    m_stderr.append(bytes);
    if (m_operation != Operation::Chat) {
        const QString text = QString::fromUtf8(bytes).trimmed();
        if (!text.isEmpty()) emit AuthenticationOutput(m_activeProvider, text);
    }
}

void AiAgentService::ParseChatLine(const QByteArray& line) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        // Some CLI warnings are plain text. Keep them for diagnostics without
        // polluting successful assistant replies.
        m_stderr.append(line);
        m_stderr.append('\n');
        return;
    }
    if (m_activeProvider == Provider::OpenAI) ParseCodexEvent(document.object());
    else if (m_activeProvider == Provider::Claude) ParseClaudeEvent(document.object());
    else ParseGeminiEvent(document.object());
}

void AiAgentService::ParseCodexEvent(const QJsonObject& event) {
    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("thread.started")) {
        SetSessionId(m_activeProvider, event.value(QStringLiteral("thread_id")).toString());
        return;
    }
    if (type == QStringLiteral("error") || type == QStringLiteral("turn.failed")) {
        QString message = event.value(QStringLiteral("message")).toString();
        if (message.isEmpty()) message = event.value(QStringLiteral("error")).toString();
        if (!message.isEmpty()) m_stderr.append(message.toUtf8() + '\n');
        return;
    }
    if (!type.startsWith(QStringLiteral("item."))) return;

    const QJsonObject item = event.value(QStringLiteral("item")).toObject();
    const QString itemType = item.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("item.completed") &&
        itemType == QStringLiteral("agent_message")) {
        const QString text = item.value(QStringLiteral("text")).toString().trimmed();
        if (!text.isEmpty()) {
            if (!m_responseText.isEmpty()) m_responseText.append(QStringLiteral("\n\n"));
            m_responseText.append(text);
        }
    } else if (itemType == QStringLiteral("mcp_tool_call")) {
        emit ActivityChanged(QStringLiteral("Using RockEngine tool: %1…")
            .arg(FriendlyToolName(EventToolName(item))));
    } else if (itemType == QStringLiteral("command_execution")) {
        emit ActivityChanged(QStringLiteral("Working in the project…"));
    } else if (itemType == QStringLiteral("file_change")) {
        emit ActivityChanged(QStringLiteral("Editing project files…"));
    }
}

void AiAgentService::ParseClaudeEvent(const QJsonObject& event) {
    const QString sessionId = event.value(QStringLiteral("session_id")).toString();
    if (!sessionId.isEmpty()) SetSessionId(m_activeProvider, sessionId);

    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("assistant")) {
        const QJsonArray content = event.value(QStringLiteral("message"))
                                       .toObject().value(QStringLiteral("content")).toArray();
        for (const QJsonValue& value : content) {
            const QJsonObject block = value.toObject();
            if (block.value(QStringLiteral("type")).toString() == QStringLiteral("tool_use")) {
                emit ActivityChanged(QStringLiteral("Using %1…")
                    .arg(FriendlyToolName(block.value(QStringLiteral("name")).toString())));
            }
        }
    } else if (type == QStringLiteral("result")) {
        const QString result = event.value(QStringLiteral("result")).toString().trimmed();
        if (!result.isEmpty()) m_responseText = result;
        if (event.value(QStringLiteral("is_error")).toBool())
            m_stderr.append(result.toUtf8() + '\n');
    }
}

void AiAgentService::ParseGeminiEvent(const QJsonObject& event) {
    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("init")) {
        SetSessionId(m_activeProvider, event.value(QStringLiteral("session_id")).toString());
        return;
    }
    if (type == QStringLiteral("message") &&
        event.value(QStringLiteral("role")).toString() == QStringLiteral("assistant")) {
        const QString content = event.value(QStringLiteral("content")).toString();
        if (event.value(QStringLiteral("delta")).toBool()) m_responseText.append(content);
        else if (!content.isEmpty()) m_responseText = content;
    } else if (type == QStringLiteral("tool_use")) {
        QString toolName = event.value(QStringLiteral("tool_name")).toString();
        if (toolName.isEmpty()) toolName = event.value(QStringLiteral("name")).toString();
        emit ActivityChanged(QStringLiteral("Using %1…")
            .arg(FriendlyToolName(toolName)));
    } else if (type == QStringLiteral("error")) {
        QString message = event.value(QStringLiteral("message")).toString();
        if (message.isEmpty()) message = event.value(QStringLiteral("error")).toString();
        if (!message.isEmpty()) m_stderr.append(message.toUtf8() + '\n');
    } else if (type == QStringLiteral("result")) {
        if (m_responseText.isEmpty()) {
            m_responseText = event.value(QStringLiteral("response")).toString();
        }
        if (event.value(QStringLiteral("status")).toString() == QStringLiteral("error")) {
            const QJsonValue error = event.value(QStringLiteral("error"));
            const QString message = error.isObject()
                ? error.toObject().value(QStringLiteral("message")).toString()
                : error.toString();
            if (!message.isEmpty()) m_stderr.append(message.toUtf8() + '\n');
        }
    }
}

void AiAgentService::ProcessFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_operation == Operation::Chat && !m_stdoutBuffer.trimmed().isEmpty())
        ParseChatLine(m_stdoutBuffer.trimmed());

    const Operation completedOperation = m_operation;
    const Provider completedProvider = m_activeProvider;
    bool success = status == QProcess::NormalExit && exitCode == 0;

    if (completedOperation == Operation::ApiKeyLogin &&
        completedProvider == Provider::Gemini && success) {
        QString vaultError;
        success = SecureCredentialStore::Store(
            QString::fromLatin1(kGeminiCredentialAccount),
            m_pendingCredential, &vaultError);
        if (!success) {
            m_stderr.append(QStringLiteral("Could not store the Gemini API key securely: %1")
                                .arg(vaultError).toUtf8());
        }
    }

    if (completedOperation == Operation::Chat) {
        if (m_cancelled) {
            emit ActivityChanged(QStringLiteral("Stopped"));
        } else if (success && !m_responseText.trimmed().isEmpty()) {
            emit ResponseReady(completedProvider, m_responseText.trimmed());
            emit ActivityChanged(QString());
        } else {
            QString error = QString::fromUtf8(m_stderr).trimmed();
            if (error.isEmpty()) {
                error = success ? QStringLiteral("The agent returned no response")
                                : QStringLiteral("The agent exited with code %1").arg(exitCode);
            }
            emit RequestFailed(completedProvider, error);
            emit ActivityChanged(QString());
        }
    } else if (completedOperation == Operation::AccountLogin ||
               completedOperation == Operation::ApiKeyLogin) {
        if (m_cancelled) {
            emit AuthenticationOutput(completedProvider, QStringLiteral("Sign-in cancelled."));
        } else if (!success) {
            QString error = QString::fromUtf8(m_stderr).trimmed();
            if (error.isEmpty() && completedProvider == Provider::Gemini) {
                const QJsonDocument output = QJsonDocument::fromJson(m_stdoutBuffer);
                const QJsonValue value = output.object().value(QStringLiteral("error"));
                error = value.isObject()
                    ? value.toObject().value(QStringLiteral("message")).toString()
                    : value.toString();
                if (error.isEmpty()) error = QString::fromUtf8(m_stdoutBuffer).trimmed();
            }
            if (error.isEmpty()) error = QStringLiteral("Sign-in did not complete");
            emit RequestFailed(completedProvider, error);
        }
    } else if (completedOperation == Operation::Logout) {
        ClearProviderCredentialFiles(completedProvider);
        if (success) {
            emit AuthenticationOutput(completedProvider,
                QStringLiteral("OpenAI credentials erased."));
        } else {
            emit RequestFailed(completedProvider,
                QStringLiteral("Codex could not erase its OS-keyring credential: %1")
                    .arg(QString::fromUtf8(m_stderr).trimmed()));
        }
    }

    FinishOperation();
    if (completedOperation != Operation::Chat)
        CheckAuthentication(completedProvider);
}

void AiAgentService::ProcessError(QProcess::ProcessError) {
    if (!m_process || m_processErrorReported || m_cancelled) return;
    m_processErrorReported = true;
    emit RequestFailed(m_activeProvider, m_process->errorString());
    // FailedToStart never emits finished, while runtime errors generally do.
    if (m_process->state() == QProcess::NotRunning) FinishOperation();
}

void AiAgentService::FinishOperation() {
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_pendingStdin.fill('\0');
    m_pendingStdin.clear();
    m_pendingCredential.fill('\0');
    m_pendingCredential.clear();
    m_stdoutBuffer.clear();
    m_stderr.clear();
    m_responseText.clear();
    m_operation = Operation::None;
    emit BusyChanged(false);
}

void AiAgentService::Cancel() {
    if (!m_process || m_operation == Operation::Logout) return;
    m_cancelled = true;
    QProcess* stopping = m_process;
    stopping->terminate();
    QTimer::singleShot(1000, stopping, [stopping]() {
        if (stopping->state() != QProcess::NotRunning) stopping->kill();
    });
}

void AiAgentService::NewConversation(Provider provider) {
    SetSessionId(provider, {});
}

void AiAgentService::Shutdown() {
    if (!m_process) return;
    disconnect(m_process, nullptr, this, nullptr);
    m_process->kill();
    m_process->waitForFinished(1000);
    delete m_process;
    m_process = nullptr;
    m_pendingStdin.fill('\0');
    m_pendingStdin.clear();
    m_pendingCredential.fill('\0');
    m_pendingCredential.clear();
    m_operation = Operation::None;
}

} // namespace ai
