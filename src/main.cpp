#include "engine/core/Application.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    Application &app = Application::Get();
    app.Init();
    app.Run();
    app.Shutdown();
    return -1;
}