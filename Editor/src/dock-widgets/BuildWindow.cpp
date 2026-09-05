#include "dock-widgets/BuildWindow.hpp"
#include "utils/GameBuilder.hpp"

#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/debug/Console.hpp"
#include "engine/jobs/JobSystem.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>

namespace fs = std::filesystem;

namespace {
    // Scenes are authored under Domain/sandbox. Scanned from disk rather than taken from
    // SceneManager::GetScenes(), which only reports what happens to be open in the editor
    // right now -- a startup scene you are not currently editing is the normal case.
    constexpr const char* kSceneDir = "Domain/sandbox";
}

BuildWindow* BuildWindow::Get() {
    static BuildWindow* instance = new BuildWindow(nullptr);
    return instance;
}

BuildWindow::BuildWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("Build Game");
    // A normal OS-decorated window, unlike SpriteEditorModal's frameless one: this is a
    // settings form the user may well want to leave open beside the editor while they
    // check something, not a light modal that should vanish on click-outside.
    setWindowFlags(Qt::Window);
    setMinimumWidth(560);
    BuildUi();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        // A pull, not a notify: JobSystem has no Observable channel on purpose (its header
        // explains why), so the UI samples it the same way LoadingOverlay does.
        const auto jobs = JobSystem::Get().SnapshotActive();
        for (const auto& j : jobs) {
            if (j.title.rfind("Building ", 0) != 0) continue;
            if (j.fraction < 0.0f) {
                m_progress->setRange(0, 0);            // indeterminate sweep
            } else {
                m_progress->setRange(0, 100);
                m_progress->setValue(static_cast<int>(j.fraction * 100.0f));
            }
            if (!j.label.empty())
                m_status->setText(QString::fromStdString(j.label));
            return;
        }
    });
}

void BuildWindow::BuildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // --- Game -------------------------------------------------------------------------
    auto* gameBox = new QGroupBox("Game", this);
    auto* gameForm = new QFormLayout(gameBox);

    m_gameName = new QLineEdit(gameBox);
    m_gameName->setPlaceholderText("My Game");
    gameForm->addRow("Name", m_gameName);

    m_version = new QLineEdit(gameBox);
    m_version->setPlaceholderText("1.0.0");
    gameForm->addRow("Version", m_version);

    m_startupScene = new QComboBox(gameBox);
    m_startupScene->setToolTip("The scene the game loads on launch.");
    gameForm->addRow("Startup Scene", m_startupScene);

    root->addWidget(gameBox);

    // --- Display ------------------------------------------------------------------------
    auto* dispBox = new QGroupBox("Display", this);
    auto* dispForm = new QFormLayout(dispBox);

    auto* resRow = new QHBoxLayout();
    m_width = new QSpinBox(dispBox);
    m_width->setRange(1, 16384);
    m_width->setValue(1280);
    m_height = new QSpinBox(dispBox);
    m_height->setRange(1, 16384);
    m_height->setValue(720);
    resRow->addWidget(m_width);
    resRow->addWidget(new QLabel("x", dispBox));
    resRow->addWidget(m_height);
    resRow->addStretch();
    dispForm->addRow("Window Size", resRow);

    auto* flagRow = new QHBoxLayout();
    m_fullscreen = new QCheckBox("Fullscreen", dispBox);
    m_resizable  = new QCheckBox("Resizable", dispBox);
    m_resizable->setChecked(true);
    m_vsync      = new QCheckBox("VSync", dispBox);
    m_vsync->setChecked(true);
    m_vsync->setToolTip("VSync paces the frame loop. Turning it off lets the game run "
                        "unbounded and spin the GPU.");
    flagRow->addWidget(m_fullscreen);
    flagRow->addWidget(m_resizable);
    flagRow->addWidget(m_vsync);
    flagRow->addStretch();
    dispForm->addRow("", flagRow);

    root->addWidget(dispBox);

    // --- Output --------------------------------------------------------------------------
    auto* outBox = new QGroupBox("Output", this);
    auto* outForm = new QFormLayout(outBox);

    m_platform = new QComboBox(outBox);
    m_platform->addItem("Windows x64");
    m_platform->setToolTip("Only the host platform can be built. Cross-compiling to macOS "
                           "or Linux from Windows is not supported.");
    outForm->addRow("Platform", m_platform);

    auto* dirRow = new QHBoxLayout();
    m_outputDir = new QLineEdit(outBox);
    auto* browse = new QPushButton("Browse...", outBox);
    dirRow->addWidget(m_outputDir, 1);
    dirRow->addWidget(browse);
    outForm->addRow("Folder", dirRow);

    m_summary = new QLabel(outBox);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outForm->addRow("Builds to", m_summary);

    root->addWidget(outBox);

    // --- Progress + actions ----------------------------------------------------------------
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* btnRow = new QHBoxLayout();
    m_openBtn = new QPushButton("Open Folder", this);
    m_openBtn->setVisible(false);
    m_buildBtn = new QPushButton("Build", this);
    m_buildRunBtn = new QPushButton("Build and Run", this);
    m_buildBtn->setDefault(true);
    btnRow->addWidget(m_openBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_buildRunBtn);
    btnRow->addWidget(m_buildBtn);
    root->addLayout(btnRow);

    // --- Wiring ------------------------------------------------------------------------------
    connect(browse, &QPushButton::clicked, this, [this]() {
        // The static helper, not an owned QDialog we exec() ourselves. It runs the platform's
        // own modal loop rather than re-entering ours, so the frame-loop re-entrancy problem
        // documented on this class does not apply to it.
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Choose an output folder", m_outputDir->text());
        if (!dir.isEmpty()) {
            m_outputDir->setText(dir);
            RefreshSummary();
        }
    });
    connect(m_gameName, &QLineEdit::textChanged, this, [this]() { RefreshSummary(); });
    connect(m_outputDir, &QLineEdit::textChanged, this, [this]() { RefreshSummary(); });
    connect(m_buildBtn, &QPushButton::clicked, this, [this]() { StartBuild(false); });
    connect(m_buildRunBtn, &QPushButton::clicked, this, [this]() { StartBuild(true); });
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_lastOutputDir.empty())
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QString::fromStdString(m_lastOutputDir)));
    });
}

std::string BuildWindow::SettingsPath() {
    // Project data, so it lives with the project and is committed. GetAssetPath keeps it
    // correct whether the editor runs from the source tree or a bundle.
    return EngineUtils::GetAssetPath("Domain/sandbox/project.build");
}

void BuildWindow::RefreshSceneList() {
    const QString previous = m_startupScene->currentData().toString();
    m_startupScene->clear();

    std::error_code ec;
    const fs::path dir = fs::path(EngineUtils::GetAssetPath(kSceneDir));
    if (fs::exists(dir, ec)) {
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() != ".scene") continue;
            // Stored asset-relative so the value survives being copied into a build --
            // an absolute path here would name a folder on this machine only.
            const std::string rel = EngineUtils::ToAssetRelative(entry.path().string());
            m_startupScene->addItem(QString::fromStdString(entry.path().filename().string()),
                                    QString::fromStdString(rel));
        }
    }

    if (m_startupScene->count() == 0) {
        m_startupScene->addItem("(no .scene files found in " + QString(kSceneDir) + ")", "");
        m_startupScene->setEnabled(false);
    } else {
        m_startupScene->setEnabled(true);
        const int idx = m_startupScene->findData(previous);
        if (idx >= 0) m_startupScene->setCurrentIndex(idx);
    }
}

void BuildWindow::RefreshSummary() {
    const std::string parent = m_outputDir->text().toStdString();
    if (parent.empty()) {
        m_summary->setText("(choose an output folder)");
        return;
    }
    // Through GameBuilder so the preview and the build agree on name sanitizing.
    QString name = m_gameName->text().trimmed();
    if (name.isEmpty()) name = "My Game";
    const std::string dir = GameBuilder::OutputDirFor(name.toStdString(), parent);
    m_summary->setText(QString::fromStdString(dir));
}

void BuildWindow::LoadSettings() {
    BuildConfig cfg;
    BuildConfig::Load(SettingsPath(), cfg);   // absent file -> defaults, which is fine

    m_gameName->setText(QString::fromStdString(cfg.gameName));
    m_version->setText(QString::fromStdString(cfg.version));
    m_width->setValue(cfg.windowWidth);
    m_height->setValue(cfg.windowHeight);
    m_fullscreen->setChecked(cfg.fullscreen);
    m_resizable->setChecked(cfg.resizable);
    m_vsync->setChecked(cfg.vsync);

    RefreshSceneList();
    if (!cfg.startupScene.empty()) {
        const int idx = m_startupScene->findData(QString::fromStdString(cfg.startupScene));
        if (idx >= 0) m_startupScene->setCurrentIndex(idx);
    }

    // The output folder is machine state, not project state -- a teammate's Downloads path
    // has no business in a committed file -- so it comes from QSettings and defaults to the
    // OS's own Downloads location.
    QSettings settings("Rocklyn", "RockEngineEditor");
    const QString saved = settings.value("build/outputDir").toString();
    m_outputDir->setText(saved.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        : saved);

    restoreGeometry(settings.value("build/geometry").toByteArray());
    RefreshSummary();
}

void BuildWindow::SaveSettings() {
    BuildConfig::Save(SettingsPath(), ConfigFromUi());

    QSettings settings("Rocklyn", "RockEngineEditor");
    settings.setValue("build/outputDir", m_outputDir->text());
    settings.setValue("build/geometry", saveGeometry());
}

BuildConfig BuildWindow::ConfigFromUi() const {
    BuildConfig cfg;
    cfg.gameName     = m_gameName->text().trimmed().toStdString();
    if (cfg.gameName.empty()) cfg.gameName = "My Game";
    cfg.version      = m_version->text().trimmed().toStdString();
    cfg.startupScene = m_startupScene->currentData().toString().toStdString();
    cfg.windowWidth  = m_width->value();
    cfg.windowHeight = m_height->value();
    cfg.fullscreen   = m_fullscreen->isChecked();
    cfg.resizable    = m_resizable->isChecked();
    cfg.vsync        = m_vsync->isChecked();
    return cfg;
}

void BuildWindow::SetBusy(bool busy) {
    m_building = busy;
    m_buildBtn->setEnabled(!busy);
    m_buildRunBtn->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (busy) {
        m_openBtn->setVisible(false);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        m_pollTimer->start();
    } else {
        m_pollTimer->stop();
    }
}

void BuildWindow::StartBuild(bool runAfter) {
    // Building while the game is running would capture the runtime container's throwaway
    // state into project.build, and Domain/ could be mid-write from a script. Refuse rather
    // than ship something the user did not author.
    Container* active = Engine::Get()->GetActiveContainer();
    if (active && active->GetMode() != Container::Mode::Editor) {
        m_status->setText("<span style='color:#e06c60;'>Stop play mode before building.</span>");
        return;
    }

    SaveSettings();

    GameBuilder::Inputs inputs;
    std::string error;
    if (!GameBuilder::Prepare(ConfigFromUi(), m_outputDir->text().toStdString(),
                              inputs, error)) {
        m_status->setText("<span style='color:#e06c60;'>" +
                          QString::fromStdString(error).toHtmlEscaped() + "</span>");
        return;
    }

    if (!inputs.playerIsExportTemplate) {
        Console::Warn("Build: no export-template player found in build/release/bin, so this "
                      "build uses the one from the editor's own tree. That binary links the "
                      "same MSVC runtime the editor does -- from a Debug editor, the debug "
                      "CRT, which ships only with Visual Studio and cannot be redistributed, "
                      "so the game will not start on anyone else's machine. Build the "
                      "shippable player with: cmake --preset windows-msvc-release && "
                      "cmake --build --preset windows-msvc-release");
    }

    if (inputs.pythonRuntime.empty()) {
        Console::Warn("Build: no staged Python runtime found (bin/player-runtime/python). "
                      "The game will only run on machines that already have Python "
                      "installed. Rebuild the RockEnginePlayer target to stage it.");
    }

    SetBusy(true);
    m_status->setText("Building...");

    GameBuilder::Submit(std::move(inputs), [this, runAfter](const GameBuilder::Result& r) {
        SetBusy(false);
        if (!r.ok) {
            m_status->setText("<span style='color:#e06c60;'>" +
                              QString::fromStdString(r.error).toHtmlEscaped() + "</span>");
            Console::Alert("Build failed: " + r.error);
            return;
        }

        m_lastOutputDir = r.outputDir;
        m_openBtn->setVisible(true);
        m_status->setText("<span style='color:#7ec87e;'>Build succeeded.</span>");
        Console::Comment("Build succeeded: " + r.outputDir);

        if (runAfter && !r.exePath.empty()) {
            // startDetached, not start(): the game outlives this editor session, and a
            // QProcess owned by the window would be killed when the window closes. Working
            // directory is the build folder so relative paths behave as they will for a
            // real player.
            QProcess::startDetached(QString::fromStdString(r.exePath), {},
                                    QString::fromStdString(r.outputDir));
        }
    });
}

void BuildWindow::ShowCentered() {
    LoadSettings();
    show();
    raise();
    activateWindow();
}

void BuildWindow::closeEvent(QCloseEvent* e) {
    // A build keeps running if the window closes -- it is a JobSystem job, not owned by
    // this widget -- but the poll timer must stop or it ticks against a hidden window.
    m_pollTimer->stop();
    SaveSettings();
    e->accept();
}
