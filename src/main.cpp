#include "engine/Engine.hpp"
#include "editor/Editor.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    
    Engine &engine = Engine::Get();
    Editor &editor = Editor::Get();
    editor.Init();
    engine.Init();
    editor.Start();
    engine.Shutdown();
    return -1;
}