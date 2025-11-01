#include "Editor.hpp"
#include <QApplication>
#include <QTimer>
#include "mainwindow-gui/MainWindowGui.hpp"
#include "editor/console-gui/ConsoleGui.hpp"
void Editor::Init() {
    int argc = 0;
    char** argv = nullptr;
    app = new QApplication(argc, argv);
    MainWindow& main_window = *MainWindow::Get();
    main_window.Init();

    timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, [this]() {
        Engine::Get().Run();
    });

    timer->start(16);
    app->exec();
}

void Editor::Update() {

}

void Editor::Shutdown() {
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
