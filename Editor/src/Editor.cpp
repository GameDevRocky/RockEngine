#include "Editor.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QStringList>
#include <QSurfaceFormat>
// QOpenGLWidget comes transitively from dock-widgets/*.hpp below, which
// include it via ViewportWidget.hpp -- that header orders glad.h first, so
// QOpenGLWidget must never be included directly before it in this file.
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include "dock-widgets/HierarchyGui.hpp"
#include <QCoreApplication>
#include "engine/jobs/JobSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/input/GamepadService.hpp"
#include "mcp/McpServer.hpp"
#include "utils/GizmoUndoBridge.hpp"
#include "utils/LoadingOverlay.hpp"
#include "utils/StartupSplash.hpp"
#include <algorithm>

Editor::Editor(){
    QCoreApplication::setOrganizationName("Rocklyn");
    QCoreApplication::setApplicationName("RockEngineEditor");

}

void Editor::Init() {
    QStringList styles = QStyleFactory::keys();
    for (auto& s : styles){
        std::cout << s.toStdString() << std::endl;
    }
    QSurfaceFormat format;
    format.setVersion(4, 6);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);   // 4x MSAA
    format.setSwapInterval(1); // vsync: pace buffer swaps to the display refresh
    QSurfaceFormat::setDefaultFormat(format);

    if (!QApplication::instance()) {
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        static int argc = 1;
        static char arg0[] = "RockEngineEditor";
        static char* argv[] = { arg0, nullptr };

        app = new QApplication(argc, argv);
    } else {
        app = qobject_cast<QApplication*>(QApplication::instance());
    }
#ifdef Q_OS_WIN
    // Fixedsys is a legacy GDI raster font that DirectWrite cannot load. Qt may
    // request it as Windows' stock fixed-font fallback, so redirect only that
    // family to a modern monospace font before any editor widgets are created.
    QFont::insertSubstitution(QStringLiteral("Fixedsys"),
                              QStringLiteral("Consolas"));
#endif
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Gamepads are POLLED from the OS, not delivered as Qt events, so unlike the keyboard they
    // keep reporting while the editor sits in the background -- a game left in play mode would
    // happily respond to stick input while you work in another application, and a rumble left
    // running would keep buzzing. Qt's application-state signal is the only thing that knows
    // about focus (Engine has no windows), so the editor pushes it down to the engine here.
    // ApplicationActive is the sole "we have the user's attention" state; Inactive, Suspended
    // and Hidden all mean the same thing to a controller.
    QObject::connect(app, &QGuiApplication::applicationStateChanged,
                     app, [](Qt::ApplicationState state) {
        GamepadService::Get().SetApplicationFocused(state == Qt::ApplicationActive);
    });

    const QStringList styleCandidates = {
        QString::fromStdString(EngineUtils::GetAssetPath("Domain/lib/assets/styling/default.qss")),
        QCoreApplication::applicationDirPath() + "/../../../Domain/lib/assets/styling/default.qss",
        QCoreApplication::applicationDirPath() + "/../../../../Domain/lib/assets/styling/default.qss"
    };

    bool styleLoaded = false;
    for (const QString& stylePath : styleCandidates) {
        QFileInfo styleInfo(stylePath);
        if (!styleInfo.exists())
            continue;

        QFile styleFile(styleInfo.absoluteFilePath());
        if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString styleSheet = QString::fromUtf8(styleFile.readAll());
            const QDir iconDirectory(
                styleInfo.dir().absoluteFilePath(QStringLiteral("../icons")));
            const auto iconPath = [&iconDirectory](const QString& filename) {
                // Qt's stylesheet parser expects a local path here. A file://
                // URL is incorrectly treated as relative on Windows.
                return QDir::fromNativeSeparators(
                    iconDirectory.absoluteFilePath(filename));
            };
            styleSheet.replace(QStringLiteral("__SCROLL_ARROW_UP__"),
                               iconPath(QStringLiteral("scroll_arrow_up.svg")));
            styleSheet.replace(QStringLiteral("__SCROLL_ARROW_DOWN__"),
                               iconPath(QStringLiteral("scroll_arrow_down.svg")));
            styleSheet.replace(QStringLiteral("__SCROLL_ARROW_LEFT__"),
                               iconPath(QStringLiteral("scroll_arrow_left.svg")));
            styleSheet.replace(QStringLiteral("__SCROLL_ARROW_RIGHT__"),
                               iconPath(QStringLiteral("scroll_arrow_right.svg")));
            app->setStyleSheet(styleSheet);
            styleLoaded = true;
            break;
        }
    }

    if (!styleLoaded) {
        std::cout << "Failed to load stylesheet: Domain/lib/assets/styling/default.qss" << std::endl;
    }

    MainWindow& main_window = *MainWindow::Get();
    main_window.Init();
    std::cout << "Editor Initialized\n" << std::endl;
}

void Editor::Update(){
    // Refresh the loading overlay from the engine's job list. A pull, not a
    // notify: JobSystem deliberately isn't an Observable, so nothing on the
    // worker side can ever end up dispatching into Qt. This runs every tick, so
    // the bar advances at the display's refresh rate for free.
    LoadingOverlay::Get()->Sync(JobSystem::Get().SnapshotActive());

    // Repaint every viewport that's currently on screen -- not just the frame
    // driver. When Scene and Game are visible at the same time (split or
    // floated), both must re-render each tick to reflect the latest engine
    // state; driving only one leaves the other frozen (and, because its ImGui
    // overlay only processes input during its own paint, makes its gizmos and
    // clicks appear dead). The frameDriver still *paces* the loop -- its
    // frameSwapped re-arms FrameTick -- but it no longer owns the repaint.
    for (QOpenGLWidget* v : viewports) {
        if (v && v->isVisible()) v->update();
    }
}

void Editor::RegisterViewport(QOpenGLWidget* w) {
    if (w && std::find(viewports.begin(), viewports.end(), w) == viewports.end())
        viewports.push_back(w);
}

void Editor::UnregisterViewport(QOpenGLWidget* w) {
    viewports.erase(std::remove(viewports.begin(), viewports.end(), w), viewports.end());
    if (frameDriver == w) frameDriver = nullptr;
}

void Editor::FrameTick(){
    Engine::Get()->Update();
    Update(); // schedule the next repaint; the driver's swap re-arms this loop
    // Stamped at the END so a long frame doesn't eat into the watchdog's stall
    // threshold: the clock measures silence between ticks, not frame duration.
    lastTickTime = std::chrono::steady_clock::now();
}

void Editor::SetFrameDriver(QOpenGLWidget* w) {
    if (frameDriver == w) return;
    if (frameDriverConn) QObject::disconnect(frameDriverConn);
    frameDriver = w;
    if (!w) return;
    frameDriverConn = QObject::connect(w, &QOpenGLWidget::frameSwapped, [this]() {
        watchdogTicking = false; // a real swap: the vsync loop is alive again
        FrameTick();
    });
    w->update(); // re-arm: the previous driver's vsync loop has already stopped
}

void Editor::PostInit() {
    std::cout << "Editor Starting ..." << std::endl;
    timer = new QTimer();

    // Subscribes to GizmosManager so viewport drags land in the undo history.
    // After Engine::PostInit, so the container and its UndoSystem exist.
    GizmoUndoBridge::Install();

    // The startup asset load (~230 metas) runs inside the first viewport's
    // initializeGL, which showMaximized below triggers -- so it happens before
    // app->exec() and before any frame is drawn. The job system cannot report it
    // (its pump rides the frame loop, which doesn't exist yet), so the splash
    // takes BootProgress callbacks and force-repaints itself instead.
    StartupSplash::Get()->Begin();

    MainWindow::Get()->PostInit();

    StartupSplash::Get()->End(MainWindow::Get());

    // After PostInit's showMaximized, so the overlay sizes to the real window
    // rect rather than a pre-layout one.
    LoadingOverlay::Get()->Attach(MainWindow::Get());

    // Local IPC endpoint for external tooling. Deliberately last: the moment this
    // listens, an external client can call in, and it must not observe a half-built
    // editor. MainWindow::PostInit above is what triggers the viewport's initializeGL
    // and therefore the whole AssetManager load, so installing any earlier gives
    // callers a window (~3s) in which the engine answers but every asset list is
    // empty. Nothing here needs to run before the window exists.
    mcp::McpServer::Install();

    // Primary clock: whichever viewport is currently visible drives the loop
    // -- its frameSwapped signal fires at the display refresh rate under
    // vsync. Each frame we advance the engine and request the next repaint,
    // which swaps again and re-fires this signal -- a self-sustaining,
    // vsync-locked frame loop. Starts on the Scene view (the initially
    // active tab); MainWindowGui switches the driver on tab changes.
    SetFrameDriver(SceneViewGui::Get());

    // Fallback watchdog: when no view is presenting (driver hidden,
    // minimized, or occluded) frameSwapped stops firing. This keeps the engine
    // ticking so scripts, file-watching, and logic never freeze. It only steps
    // when the vsync loop has clearly stalled, so it never double-drives a
    // healthy 60Hz+ loop.
    constexpr int kWatchdogIntervalMs = 16; // how often to check for a stall
    constexpr int kStallThresholdMs   = 250; // driver presenting but silent this long = stalled
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, [this]() {
        // A *slow* loop is not a *stalled* loop: while the driver is visible a
        // swap is coming, and an extra FrameTick here would pile a second full
        // Engine::Update onto exactly the frames that are already over budget.
        // Only step when no viewport can present (hidden/minimized/occluded) or
        // the visible driver has been silent for far longer than any real frame.
        const auto sinceLastTick = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - lastTickTime).count();
        const bool driverPresenting = frameDriver && frameDriver->isVisible() &&
                                      !frameDriver->window()->isMinimized() && !watchdogTicking;
        const int threshold = driverPresenting ? kStallThresholdMs : 2 * kWatchdogIntervalMs;
        if (sinceLastTick >= threshold) {
            // Once a presenting driver has been silent past the stall threshold
            // it is genuinely stuck (e.g. occluded, so Qt skips its paints);
            // latch into watchdog pacing until a real swap clears the latch.
            watchdogTicking = true;
            FrameTick();
        }
    });

    lastTickTime = std::chrono::steady_clock::now();
    timer->start(kWatchdogIntervalMs);

    app->exec();

}

void Editor::Shutdown() {
    // Before everything else: stop accepting requests and drop open connections, so no
    // external tool call can land on an engine that is already coming apart.
    mcp::McpServer::Shutdown();

    // First, before any Qt teardown. A job completing against a half-destroyed
    // MainWindow (or a deleted QApplication) is the one way this shutdown order
    // can crash, and joining here closes it -- app->exec() has already returned,
    // so no further Pump will run and any in-flight result is simply dropped.
    JobSystem::Get().Shutdown();

    MainWindow::Get()->Shutdown();

    if (timer) {
        timer->stop();
        delete timer;
        timer = nullptr;
    }
    if (app) {
        delete app;
        app = nullptr;
    }
}
