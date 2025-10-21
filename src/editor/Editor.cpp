#include "Editor.hpp"
#include <QApplication>
#include <QTimer>
#include "console-gui/ConsoleGui.hpp"

void Editor::Init() {
    int argc = 0;
    char** argv = nullptr;
    app = new QApplication(argc, argv);
    ConsoleGui& console_window = *ConsoleGui::Get();
    console_window.show();
    timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, [this]() {
        Engine::Get().Run();
    });

    timer->start(16);
    app->exec();
    delete timer;
    delete app;
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
