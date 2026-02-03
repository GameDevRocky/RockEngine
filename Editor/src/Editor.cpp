#include "Editor.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QTimer>
#include <QFile>
#include <QSurfaceFormat>
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include <QCoreApplication>


Editor::Editor(){
    QCoreApplication::setOrganizationName("Rocklyn");
    QCoreApplication::setApplicationName("RockEngineEditor");

}

void Editor::Init() {

    QSurfaceFormat format;
    format.setVersion(4, 6);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
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

    QPalette darkPalette;

    // Window background
    darkPalette.setColor(QPalette::Window, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::WindowText, Qt::white);

    // Input fields
    darkPalette.setColor(QPalette::Base, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::AlternateBase, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::Text, QColor(230, 230, 230));

    // Buttons
    darkPalette.setColor(QPalette::Button, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);

    // Highlights (selection, hover)
    darkPalette.setColor(QPalette::Highlight, QColor(80, 80, 80));   // bluish accent
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    // Tooltips
    darkPalette.setColor(QPalette::ToolTipBase, QColor(40, 40, 40));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);

    // Links
    darkPalette.setColor(QPalette::Link, QColor(100, 150, 255));

    qApp->setPalette(darkPalette);
    MainWindow& main_window = *MainWindow::Get();
    main_window.Init();
    std::cout << "Editor Initialized\n" << std::endl;
}

void Editor::Update(){
    SceneViewGui::Get()->update();
    GameViewGui::Get()->update();
    
}

void Editor::PostInit() {
    std::cout << "Editor Starting ..." << std::endl;
    timer = new QTimer();
    MainWindow::Get()->PostInit();
    Engine::Get()->LoadDefaultScene();
    QObject::connect(timer, &QTimer::timeout, [this]() {
        
        Engine::Get()->Update();
        Editor::Get()->Update();
    });
    timer->start(16);
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
