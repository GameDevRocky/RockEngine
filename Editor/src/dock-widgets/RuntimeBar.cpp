#include "dock-widgets/RuntimeBar.hpp"
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include "Engine.hpp"

void RuntimeBar::Init() {
    setFixedHeight(24);
    setContentsMargins(0,0,0,0);

    // 1. Create the Layout
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->setSpacing(10);

    // 2. Create Buttons
    QIcon playIcon = style()->standardIcon(QStyle::SP_MediaPlay);
    playButton = new QPushButton("", this);
    playButton->setIcon(playIcon);
    playButton->setFixedWidth(64);

    connect(playButton, &QPushButton::clicked, []() {
        Engine::Get()->Toggle();
    });
    
    QIcon pauseIcon = style()->standardIcon(QStyle::SP_MediaPause);
    pauseButton = new QPushButton("", this);
    pauseButton->setIcon(pauseIcon);
    pauseButton->setFixedWidth(64);

    // 3. Setup Centering with Stretches
    layout->addStretch();      // Left spacer
    layout->addWidget(playButton);
    layout->addWidget(pauseButton);
    layout->addStretch();      // Right spacer

    
}

RuntimeBar::RuntimeBar(QWidget* parent) : QWidget(parent) {

}