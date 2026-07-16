#pragma once
#include <QApplication>
#include <engine/core/System.hpp>
#include <Engine.hpp>
#include <QTimer>
#include <chrono>

class Editor : public System {
public:
    static Editor* Get() {
        static Editor* instance = new Editor();
        return instance;
    }

    void Init() override;
    void PostInit() override;
    void Update() override;
    void Shutdown() override;

    QApplication* App() const { return app; }

private:
    Editor();
    ~Editor() override = default;

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    // One simulation + render step: advances the engine, then schedules the next
    // repaint of the views. Driven by the Scene view's vsync-paced frameSwapped
    // signal, with the watchdog timer as a fallback when no view is presenting.
    void FrameTick();

    QApplication* app = nullptr;
    QTimer *timer = nullptr;
    std::chrono::steady_clock::time_point lastTickTime{};
};
