#include "engine/core/Application.hpp"
<<<<<<< HEAD
#include "editor/Editor.hpp"
=======
#include <iostream>
>>>>>>> main

int main(int argc, char *argv[]) {
    Application &app = Application::Get();
    Editor &editor = Editor::Get();
    app.Init();
    editor.Init();
    app.Run();
    app.Shutdown();
    return -1;
}