#include "engine/Engine.hpp"
#include "editor/Editor.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    
    Engine &engine = Engine::Get();
    Editor &editor = Editor::Get();
    engine.Init();
    editor.Init();
    engine.Shutdown();
    return -1;
}