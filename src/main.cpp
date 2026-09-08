#include "Engine.hpp"
#include "Editor.hpp"
#include <iostream>

#if defined(_WIN32)
// Ask hybrid-GPU drivers to create RockEngine's OpenGL contexts on the discrete
// GPU. CUDA/OpenGL interop cannot map a buffer owned by the AMD/Intel iGPU.
// Windows' per-application Graphics preference can still override this hint.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[]) {
    Engine* engine = Engine::Get();
    Editor* editor = Editor::Get();
    engine->Init();
    editor->Init();
    engine->PostInit();
    editor->PostInit();
    editor->Shutdown();
    engine->Shutdown();
    return 0;
}
