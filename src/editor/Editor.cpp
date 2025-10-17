#include "Editor.hpp"
#include "EditorWindow.hpp"
#include <QApplication>
#include <QTimer>

void Editor::Init() {
    int argc = 0;
    char** argv = nullptr;
    app = new QApplication(argc, argv);

    EditorWindow& window = EditorWindow::Get();
    window.Init();

    timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, []() {
        Application::Get().Run(); // assuming Application::Run() is a static function
    });
    timer->start(16); // roughly 60 FPS

    app->exec();
}

void Editor::Update() {
    // not needed if using QTimer + app->exec()
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
