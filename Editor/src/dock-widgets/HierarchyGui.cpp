#include "dock-widgets/HierarchyGui.hpp"
#include "utils/HierarchyTreeModel.hpp"
#include <QMenu>
#include <QPoint>
#include <QTreeView>
#include "engine/core/SceneManager.hpp"
#include "Engine.hpp"


HierarchyGui::HierarchyGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(300);
    
    
    layout = new QVBoxLayout();
    setLayout(layout);
    layout->setContentsMargins(0,0,0,0);

    CreateHeader();
    layout->addWidget(header);

    // Create tree view
    treeView = new QTreeView(this);
    treeView->setHeaderHidden(true);
    layout->addWidget(treeView);

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
    iconLabel->setFixedSize(20, 20); 
    
    filter = new QTextEdit(this);
    filter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    filter->setMaximumHeight(30);
    filter->setPlaceholderText("Filter GameObjects...");
    
    QPushButton* menuButton = new QPushButton();
    menuButton->setIcon(QIcon("domain/assets/icons/hamburger_icon.png"));
    menuButton->setFixedSize(32,32);
    menuButton->setFlat(true);

    QMenu* menu = new QMenu(header);
    menu->addAction("New Scene");
    menu->addAction("Save Scenes");
    menu->addAction("Option 3");

    connect(menuButton, &QPushButton::clicked, this, [=](){
        QPoint pos = menuButton->mapToGlobal(QPoint(0, menuButton->height()));
        menu->popup(pos);
    });
    
    header_layout->addWidget(iconLabel);
    header_layout->addWidget(filter);
    header_layout->addWidget(menuButton);
    header->setLayout(header_layout);
}

void HierarchyGui::Init(){ 
    std::cout << "HierarchyGui Initialized" << std::endl;
}

void HierarchyGui::Start(){
    Engine* engine = Engine::Get();
    SceneManager* sceneManager = engine->GetActiveContainer()->FindSystem<SceneManager>();
    
    std::vector<Scene*> scenes = sceneManager->GetScenes();
    if (!scenes.empty()) {
        SetScene(scenes[0]);
        
    }
    else{
        std::cout << "No Scenes" << std::endl;
    }
}

void HierarchyGui::SetScene(Scene* scene) {
    if (!scene || scene == currentScene)
        return;

    currentScene = scene;

    
    if (!treeModel) {
        treeModel = new HierarchyTreeModel(scene, this);
        treeView->setModel(treeModel);
    } else {
        // Update existing model with new scene
        treeModel->SetScene(scene);
    }
}