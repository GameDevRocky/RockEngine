#include "Editor.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QSurfaceFormat>
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/ConsoleGui.hpp"
#include "dock-widgets/SceneViewGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include "dock-widgets/HierarchyGui.hpp"
#include <QCoreApplication>


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

    const QStringList styleCandidates = {
        "Domain/lib/assets/styling/default.qss",
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
    SceneViewGui::Get()->update();
    GameViewGui::Get()->update();
    HierarchyGui::Get()->RefreshHierarchy();
    
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
