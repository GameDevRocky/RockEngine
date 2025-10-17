#pragma once
#include <QApplication>
#include <engine/core/System.hpp>
#include <engine/core/Application.hpp>
#include <QTimer>

class Editor : public System {
public:
    static Editor& Get() {
        static Editor instance;
        return instance;
    }

    void Init() override;
    void Update() override;
    void Shutdown() override;
    
    QApplication* App() const { return app; }

private:
    Editor() = default;
    ~Editor() override = default;

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    QApplication* app = nullptr;
    QTimer *timer = nullptr;
};
