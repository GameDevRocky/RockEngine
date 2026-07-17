#pragma once
#include <QApplication>
#include <QObject>
#include <engine/core/System.hpp>
#include <Engine.hpp>
#include <QTimer>
#include <chrono>

class QOpenGLWidget;

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

    // Whichever viewport is currently visible drives the frame loop -- only
    // the driver gets its frameSwapped connected, and only the driver is
    // update()'d each tick. Without this, only the Scene view's tab drove
    // FrameTick, so switching to the Game tab (e.g. entering play mode)
    // stopped the vsync loop entirely and dropped the whole engine to the
    // ~32ms watchdog (~31 FPS) for as long as the Game tab was active.
    void SetFrameDriver(QOpenGLWidget* w);

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

    QOpenGLWidget* frameDriver = nullptr;
    QMetaObject::Connection frameDriverConn;
};
