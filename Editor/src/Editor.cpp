#include "Editor.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
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
#include "engine/utils/EngineUtils.hpp"
#include "utils/GizmoUndoBridge.hpp"
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
    QApplication::setStyle(QStyleFactory::create("Fusion"));

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
            app->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
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

    MainWindow::Get()->PostInit();

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
