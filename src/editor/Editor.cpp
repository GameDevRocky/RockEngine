#include "Editor.hpp"

void Editor::Init() {
    if (!app) {
        int argc = 0;
        char** argv = nullptr;
        app = new QApplication(argc, argv);
    }
}

void Editor::Update() {
    if (app)
        app->processEvents();
}

void Editor::Shutdown() {
    delete app;
    app = nullptr;
}

