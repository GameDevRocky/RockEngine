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
        auto* container = Engine::Get()->GetActiveContainer();
        py::gil_scoped_acquire gil;
        if (container->GetMode() == Container::Mode::Editor){
            Engine::Get()->EnterPlayMode();
            runtimeButton->setIcon(*stopIcon);
            MainWindow::Get()->central_tabs->setCurrentWidget(GameViewGui::Get());
            GameViewGui::Get()->setFocus();
        }
        else if (container->GetMode() == Container::Mode::Runtime){
            Engine::Get()->ExitPlayMode();
            runtimeButton->setIcon(*playIcon);
        }
    });
    
    QIcon* pauseIcon = new QIcon(GetAssetPath("Domain/lib/assets/icons/pause_icon.png").c_str());
    pauseButton = new QPushButton("", this);
    pauseButton->setIcon(*pauseIcon);
    pauseButton->setFixedWidth(48);

    layout->addStretch();
    layout->addWidget(runtimeButton);
    layout->addWidget(pauseButton);
    layout->addStretch(); 

    const int contentHeight = pauseButton->sizeHint().height() + 2;
    setFixedHeight(contentHeight);
    
    std::cout << "RuntimeBar Initialized" << std::endl;
}

RuntimeBar::RuntimeBar(QWidget* parent) : QWidget(parent) {

}