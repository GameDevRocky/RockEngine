#include "engine/core/Application.hpp"
#include "editor/Editor.hpp"

int main(int argc, char *argv[]) {
    Application &app = Application::Get();
    Editor &editor = Editor::Get();
    app.Init();
    editor.Init();
    app.Run();
    app.Shutdown();
    return -1;
}