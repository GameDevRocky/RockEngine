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
#include <QCoreApplication>


Editor::Editor(){
    QCoreApplication::setOrganizationName("Rocklyn");
    QCoreApplication::setApplicationName("RockEngineEditor");

}


void Editor::Init() {
    // Set default OpenGL format before creating QApplication
    QSurfaceFormat format;
    format.setVersion(4, 6);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    
    if (!QApplication::instance()) {
        static int argc = 0;
        static char* argv[] = { const_cast<char*>("RockEngineEditor") };
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

    // QFile file("domain/assets/styling/default.qss");  // put the file in your resources
    // if (file.open(QFile::ReadOnly | QFile::Text)) {
    //     qApp->setStyleSheet(file.readAll());
    // }

    MainWindow& main_window = *MainWindow::Get();
    main_window.Init();
}

void Editor::Update(){

    
}

void Editor::Start() {

    timer = new QTimer();
    MainWindow::Get()->Start();

    QObject::connect(timer, &QTimer::timeout, [this]() {
        Engine::Get().Run();
        SceneViewGui::Get()->update();
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
