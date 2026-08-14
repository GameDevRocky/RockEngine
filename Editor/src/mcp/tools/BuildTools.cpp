#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"

#include "engine/core/BuildConfig.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/GameBuilder.hpp"

#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace mcp {

namespace {

// Mirrors BuildWindow: the config is project data committed with the project, while the
// output folder is machine state that lives in QSettings.
std::string ProjectBuildPath() {
    return EngineUtils::GetAssetPath("Domain/sandbox/project.build");
}

QString DefaultOutputDir() {
    QSettings settings("Rocklyn", "RockEngineEditor");
    const QString saved = settings.value("build/outputDir").toString();
    return saved.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                           : saved;
}

// The last finished build, so build.get_status can still answer after the job is gone
// from JobSystem's active list.
struct LastBuild {
    bool        ran = false;
    bool        ok = false;
    std::string error;
    std::string outputDir;
    std::string exePath;
};

LastBuild& Last() {
    static LastBuild last;
    return last;
}

bool& InFlight() {
    static bool inFlight = false;
    return inFlight;
}

} // namespace

void RegisterBuildTools(McpDispatcher& dispatcher) {
    // Submits and returns. The copy runs on a JobSystem worker -- the one operation here
    // that must not be done inline, since a socket handler blocking for the length of a
    // build would freeze the whole editor (see McpServer.hpp).
    dispatcher.RegisterTool("build.trigger", [](const QJsonObject& params) {
        // Same refusal BuildWindow makes: building from play mode would capture the
        // throwaway runtime world rather than what the user authored.
        if (support::IsRuntimeWorld())
            return McpResult::Error(WrongMode, "stop play mode before building");
        if (InFlight())
            return McpResult::Error(BuildInProgress, "a build is already running");

        BuildConfig config;
        BuildConfig::Load(ProjectBuildPath(), config);   // absent file -> defaults

        const QString outputParent = params.value("outputDir").toString().isEmpty()
            ? DefaultOutputDir()
            : params.value("outputDir").toString();

        GameBuilder::Inputs inputs;
        std::string error;
        if (!GameBuilder::Prepare(config, outputParent.toStdString(), inputs, error))
            return McpResult::Error(WrongMode, QString::fromStdString(error));

        const bool runAfter = params.value("runAfter").toBool(false);

        InFlight() = true;
        Last() = LastBuild{};
        GameBuilder::Submit(std::move(inputs), [runAfter](const GameBuilder::Result& result) {
            InFlight() = false;
            Last() = LastBuild{true, result.ok, result.error, result.outputDir, result.exePath};

            if (!result.ok) {
                Console::Alert("Build failed (MCP): " + result.error);
                return;
            }
            Console::Comment("Build succeeded (MCP): " + result.outputDir);
            if (runAfter && !result.exePath.empty()) {
                QProcess::startDetached(QString::fromStdString(result.exePath), {},
                                        QString::fromStdString(result.outputDir));
            }
        });

        QJsonObject data;
        data["status"] = "build_submitted";
        data["gameName"] = QString::fromStdString(config.gameName);
        data["outputDir"] = QString::fromStdString(
            GameBuilder::OutputDirFor(config.gameName, outputParent.toStdString()));
        data["poll"] = "build.get_status";
        return McpResult::Ok(data);
    });

    dispatcher.RegisterTool("build.get_status", [](const QJsonObject&) {
        QJsonObject data;
        data["inProgress"] = InFlight();

        const LastBuild& last = Last();
        if (!InFlight() && last.ran) {
            data["ok"] = last.ok;
            data["outputDir"] = QString::fromStdString(last.outputDir);
            data["exePath"] = QString::fromStdString(last.exePath);
            if (!last.ok)
                data["error"] = QString::fromStdString(last.error);
        }
        return McpResult::Ok(data);
    });

    // Launch a build that already exists, without rebuilding it.
    dispatcher.RegisterTool("build.run", [](const QJsonObject&) {
        const LastBuild& last = Last();
        if (!last.ran || !last.ok || last.exePath.empty())
            return McpResult::Error(ObjectNotFound, "no successful build to run this session");

        // Detached so the game outlives the editor session, as BuildWindow does.
        if (!QProcess::startDetached(QString::fromStdString(last.exePath), {},
                                     QString::fromStdString(last.outputDir)))
            return McpResult::Error(WrongMode, "could not launch " +
                                               QString::fromStdString(last.exePath));

        QJsonObject data;
        data["launched"] = QString::fromStdString(last.exePath);
        return McpResult::Ok(data);
    });
}

} // namespace mcp
