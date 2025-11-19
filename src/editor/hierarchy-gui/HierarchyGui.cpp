#include "HierarchyGui.hpp"


HierarchyGui::HierarchyGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(300);


    layout = new QVBoxLayout();
    setLayout(layout);
    layout->setContentsMargins(0,0,0,0);

    CreateHeader();
    
    layout->addWidget(header);

    layout->addStretch();

}

void HierarchyGui::CreateHeader(){

    header = new QWidget();
    header->setStyleSheet(
    "border-bottom: 1px solid #454545;"
    );


    
    QHBoxLayout* header_layout = new QHBoxLayout();
    header_layout->setSpacing(4);
    header_layout->setContentsMargins(8,4,4,4);
    
    QPixmap searchPix("domain/assets/icons/search_icon.png");
    searchPix = searchPix.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(searchPix);
    iconLabel->setFixedSize(20, 20); // keeps layout consistent
    
    
    filter = new QTextEdit(this);
    filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    filter->setMaximumHeight(30); // small height like a search bar
    filter->setPlaceholderText("Filter GameObjects...");
    
    QPushButton* menuButton = new QPushButton();
    menuButton->setIcon(QIcon("domain/assets/icons/hamburger_icon.png"));
    menuButton->setFixedSize(32,32);
    menuButton->setFlat(true);
    
    
    
    header_layout->addWidget(iconLabel);
    header_layout->addWidget(filter);
    header_layout->addWidget(menuButton);
    header->setLayout(header_layout);
}

void HierarchyGui::Init(){ 
   
    
}