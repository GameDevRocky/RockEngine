#include "dock-widgets/HierarchyGui.hpp"
#include "utils/HierarchyTreeModel.hpp"
#include <QMenu>
#include <QPoint>
#include <QTreeView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include "engine/core/SceneManager.hpp"
#include "Engine.hpp"
#include "engine/debug/Console.hpp"


HierarchyGui::HierarchyGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(300);
    layout = new QVBoxLayout();
    setLayout(layout);
    layout->setContentsMargins(0,0,0,0);

    CreateHeader();
    layout->addWidget(header);

    treeView = new QTreeView(this);
    //treeView->setHeaderHidden(true);
    layout->addWidget(treeView);
    setAcceptDrops(true);
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
    menuButton->setIcon(QIcon("Domain/lib/assets/icons/hamburger_icon.png"));
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

void HierarchyGui::PostInit(){
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

void HierarchyGui::dragEnterEvent(QDragEnterEvent* event) {
    // Check if the drag contains file URLs
    if (event->mimeData()->hasUrls()) {
        // Check if any of the URLs is a .yaml file
        bool hasYamlFile = false;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (filePath.endsWith(".yaml", Qt::CaseInsensitive) || 
                    filePath.endsWith(".yml", Qt::CaseInsensitive)) {
                    hasYamlFile = true;
                    break;
                }
            }
        }
        
        if (hasYamlFile) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void HierarchyGui::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        bool hasYamlFile = false;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (filePath.endsWith(".yaml", Qt::CaseInsensitive) || 
                    filePath.endsWith(".yml", Qt::CaseInsensitive)) {
                    hasYamlFile = true;
                    break;
                }
            }
        }
        
        if (hasYamlFile) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void HierarchyGui::dropEvent(QDropEvent* event) {
    // Clear visual feedback
    setStyleSheet("");
    
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    
    // Get the first .yaml file from the dropped files
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
            
        QString filePath = url.toLocalFile();
        
        // Check if it's a YAML file
        if (!filePath.endsWith(".yaml", Qt::CaseInsensitive) && 
            !filePath.endsWith(".yml", Qt::CaseInsensitive))
            continue;
        
        // Convert to std::string
        std::string sceneFilePath = filePath.toStdString();
        
        // Load the scene using SceneManager
        Engine* engine = Engine::Get();
        if (engine && engine->GetActiveContainer()) {
            SceneManager* sceneManager = engine->GetActiveContainer()->FindSystem<SceneManager>();
            if (sceneManager) {
                Console::Comment("Loading scene from: " + sceneFilePath);
                sceneManager->LoadScene(sceneFilePath);
                
                // Update hierarchy to show the new scene
                std::vector<Scene*> scenes = sceneManager->GetScenes();
                if (!scenes.empty()) {
                    SetScene(scenes.back()); // Set to the newly loaded scene
                }
                
                event->acceptProposedAction();
                return;
            } else {
                Console::Alert("SceneManager not found!");
            }
        } else {
            Console::Alert("Engine or active container not available!");
        }
    }
    
    event->ignore();
}

void HierarchyGui::dragLeaveEvent(QDragLeaveEvent* event) {
    // Clear visual feedback
    setStyleSheet("");
    event->accept();
}