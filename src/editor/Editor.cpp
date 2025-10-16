#include "editor/Editor.hpp"

void Editor::Init() override {
    if (!app) {
        int argc = 0;
        char** argv = nullptr;
        app = new QApplication(argc, argv);
    }
}

void Editor::Update() override {
    if (app)
        app->processEvents(); // Keeps the Qt event loop alive
}

void Editor::Shutdown() override {
    delete app;
    app = nullptr;
}

