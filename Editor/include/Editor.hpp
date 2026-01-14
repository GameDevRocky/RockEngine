#pragma once
#include <QApplication>
#include <engine/core/System.hpp>
#include <Engine.hpp>
#include <QTimer>

class Editor : public System {
public:
    static Editor* Get() {
        static Editor* instance = new Editor();
        return instance;
    }

    void Init() override;
    void Start();
    void Update() override;
    void Shutdown() override;
    
    QApplication* App() const { return app; }

private:
    Editor();
    ~Editor() override = default;

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    QApplication* app = nullptr;
    QTimer *timer = nullptr;
};
