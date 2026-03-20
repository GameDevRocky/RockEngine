#include "dock-widgets/RuntimeBar.hpp"
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include "Engine.hpp"
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/GameViewGui.hpp"

void RuntimeBar::Init() {
    setObjectName("RuntimeBar");
    setContentsMargins(0,0,0,0);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    

    QIcon* playIcon = new QIcon("Domain/lib/assets/icons/play_icon.png");
    playButton = new QPushButton("", this);
    playButton->setIcon(*playIcon);
    playButton->setFixedWidth(48);
    
    connect(playButton, &QPushButton::clicked, []() {
        Engine::Get()->EnterPlayMode();
        MainWindow::Get()->central_tabs->setCurrentWidget(GameViewGui::Get());
        GameViewGui::Get()->setFocus();
    });
    
    QIcon* pauseIcon = new QIcon("Domain/lib/assets/icons/pause_icon.png");
    pauseButton = new QPushButton("", this);
    pauseButton->setIcon(*pauseIcon);
    pauseButton->setFixedWidth(48);

    layout->addStretch();
    layout->addWidget(playButton);
    layout->addWidget(pauseButton);
    layout->addStretch(); 

    const int contentHeight = pauseButton->sizeHint().height();
    setFixedHeight(contentHeight);
    
    std::cout << "RuntimeBar Initialized" << std::endl;
}

RuntimeBar::RuntimeBar(QWidget* parent) : QWidget(parent) {

}