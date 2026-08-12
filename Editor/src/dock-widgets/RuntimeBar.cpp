#include <pybind11/pybind11.h>
#include <pybind11/gil.h>
#include <pybind11/embed.h> 
#include "dock-widgets/RuntimeBar.hpp"
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include "Engine.hpp"
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/GameViewGui.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;


namespace py = pybind11;

void RuntimeBar::Init() {
    setObjectName("RuntimeBar");
    setContentsMargins(0,8,0,0);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    playIcon = new QIcon(GetAssetPath("Domain/lib/assets/icons/play_icon.png").c_str());
    stopIcon = new QIcon(GetAssetPath("Domain/lib/assets/icons/stop_icon.png").c_str());
    runtimeButton = new QPushButton("", this);
    runtimeButton->setIcon(*playIcon);
    runtimeButton->setFixedWidth(48);
    connect(runtimeButton, &QPushButton::clicked, [this]() {
        // No py::gil_scoped_acquire here any more. It used to wrap the whole
        // handler, but the transition is now deferred to a job, so the scope
        // would close before any Python ran -- leaving a lock that looked
        // load-bearing and wasn't. The code that actually needs the GIL
        // (ScriptComponent::InstantiateScript) takes it itself.
        auto* container = Engine::Get()->GetActiveContainer();
        if (!container) return;

        // Both requests are no-ops while a transition is already in flight, so
        // a double-click can't deep-copy the world twice.
        if (container->GetMode() == Container::Mode::Editor)
            Engine::Get()->RequestEnterPlayMode();
        else if (container->GetMode() == Container::Mode::Runtime)
            Engine::Get()->RequestExitPlayMode();
    });

    // The button now follows the engine rather than the click: the transition
    // happens a frame or two after the press, so flipping the icon inline would
    // show "stop" during a period when the game hasn't started yet.
    Engine::Get()->Subscribe([this]() {
        runtimeButton->setIcon(*stopIcon);
        // Bring the Game viewport to the front (raise() surfaces it whether
        // it's tabbed behind the Scene view or floating in its own window).
        MainWindow::Get()->gameviewDock->show();
        MainWindow::Get()->gameviewDock->raise();
        GameViewGui::Get()->setFocus();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);

    Engine::Get()->Subscribe([this]() {
        runtimeButton->setIcon(*playIcon);
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);
    
    pauseIcon = new QIcon(GetAssetPath("Domain/lib/assets/icons/pause_icon.png").c_str());
    pauseButton = new QPushButton("", this);
    pauseButton->setIcon(*pauseIcon);
    pauseButton->setFixedWidth(48);
    pauseButton->setEnabled(false);
    connect(pauseButton, &QPushButton::clicked, [this]() {
        py::gil_scoped_acquire gil;
        if (Engine::Get()->IsPaused()){
            Engine::Get()->ResumeMode();
            pauseButton->setIcon(*pauseIcon);
        }
        else {
            Engine::Get()->PauseMode();
            pauseButton->setIcon(*playIcon);   // show resume glyph while paused
        }
    });

    stepIcon = new QIcon(GetAssetPath("Domain/lib/assets/icons/FastForwardIcon.png").c_str());
    stepButton = new QPushButton("", this);
    stepButton->setIcon(*stepIcon);
    stepButton->setFixedWidth(48);
    stepButton->setEnabled(false);
    connect(stepButton, &QPushButton::clicked, [this]() {
        py::gil_scoped_acquire gil;
        if (!Engine::Get()->IsPaused()){        // auto-pause a running game (Unity-style)
            Engine::Get()->PauseMode();
            pauseButton->setIcon(*playIcon);
        }
        Engine::Get()->StepFrame();
    });

    layout->addStretch();
    layout->addWidget(runtimeButton);
    layout->addWidget(pauseButton);
    layout->addWidget(stepButton);
    layout->addStretch();

    const int contentHeight = pauseButton->sizeHint().height() + 2;
    setFixedHeight(contentHeight);

    // Pause/Step are only meaningful in play mode: enable on enter, disable + reset on exit.
    Engine::Get()->Subscribe([this]() {
        pauseButton->setEnabled(true);
        stepButton->setEnabled(true);
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);
    Engine::Get()->Subscribe([this]() {
        pauseButton->setEnabled(false);
        stepButton->setEnabled(false);
        pauseButton->setIcon(*pauseIcon);
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);

    std::cout << "RuntimeBar Initialized" << std::endl;
}

RuntimeBar::RuntimeBar(QWidget* parent) : QWidget(parent) {

}