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
    if (frameDriver) frameDriver->update();
}

void Editor::FrameTick(){
    lastTickTime = std::chrono::steady_clock::now();
    Engine::Get()->Update();
    Update(); // schedule the next repaint; the driver's swap re-arms this loop
}

void Editor::SetFrameDriver(QOpenGLWidget* w) {
    if (frameDriver == w) return;
    if (frameDriverConn) QObject::disconnect(frameDriverConn);
    frameDriver = w;
    if (!w) return;
    frameDriverConn = QObject::connect(w, &QOpenGLWidget::frameSwapped, [this]() {
        FrameTick();
    });
    w->update(); // re-arm: the previous driver's vsync loop has already stopped
}

void Editor::PostInit() {
    std::cout << "Editor Starting ..." << std::endl;
    timer = new QTimer();

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
    constexpr int kStallThresholdMs   = 32; // ~2 frames at 60Hz without a swap
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, [this]() {
        const auto sinceLastTick = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - lastTickTime).count();
        if (sinceLastTick >= kStallThresholdMs) {
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
