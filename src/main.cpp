#include "Engine.hpp"
#include "Editor.hpp"
#include <iostream>


int main(int argc, char *argv[]) {

    Engine &engine = Engine::Get();
    Editor &editor = Editor::Get();
    engine.Init();
    editor.Init();
    editor.Start();
    editor.Shutdown();
    engine.Shutdown();
    return -1;
}