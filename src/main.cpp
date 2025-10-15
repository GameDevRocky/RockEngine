#include "engine/core/Application.hpp"

int main(int argc, char *argv[]) {
    Application &app = Application::Get();
    app.Init();
    app.Run();
    app.Shutdown();
    return -1;
}