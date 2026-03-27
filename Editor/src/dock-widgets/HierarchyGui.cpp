#include "dock-widgets/HierarchyGui.hpp"
#include <QMenu>
#include <QPoint>
#include <QTreeView>
#include <QHeaderView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include "engine/core/SceneManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/utils/Callback.hpp"
#include "Engine.hpp"
#include "engine/debug/Console.hpp"

#include <unordered_set>

HierarchyGui::HierarchyGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(300);
    setMaximumWidth(600);
    layout = new QVBoxLayout();
    layout->setContentsMargins(0,0,0,0);
    
    setLayout(layout);
    setAcceptDrops(true);
}

HierarchyGui::~HierarchyGui() {

}

void HierarchyGui::Init(){ 
    std::cout << "HierarchyGui Initialized" << std::endl;
}

void HierarchyGui::PostInit(){
    std::cout << "Hierarchy Post Initialized" << std::endl;
    auto* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    sceneManager->Subscribe([this](){
        this->AddSceneTree();
    }, SceneManager::LOADED_SCENE_EVENT);
    
}


void HierarchyGui::AddSceneTree() {
    auto* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    Scene* scene = sceneManager->GetScenes().back();
    
    SceneTree* tree = new SceneTree();
    sceneTrees[scene->GetID()] = tree;
    std::string id = scene->GetID();
    tree->RebuildFromScene(scene);
    scene->Subscribe([tree, id](){
        auto* scene = Registry::FindInRuntime<Scene>(id);
        tree->RebuildFromScene(scene);
    }, Scene::HIERARCHY_CHANGED_EVENT);
    this->layout->addWidget(tree);
}
void HierarchyGui::RemoveSceneTree() {
    
    
}
void HierarchyGui::RefreshHierarchy() {
    auto* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    Scene* scene = sceneManager->GetScenes().back();

    SceneTree* tree = new SceneTree();
    
}


void HierarchyGui::dragEnterEvent(QDragEnterEvent* event) {
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
        
        if (!filePath.endsWith(".yaml", Qt::CaseInsensitive) && 
            !filePath.endsWith(".yml", Qt::CaseInsensitive))
            continue;
        
        std::string sceneFilePath = filePath.toStdString();
        Engine* engine = Engine::Get();
        if (engine && engine->GetActiveContainer()) {
            SceneManager* sceneManager = engine->GetActiveContainer()->FindSystem<SceneManager>();
            if (sceneManager) {
                Console::Comment("Loading scene from: " + sceneFilePath);
                sceneManager->LoadScene(sceneFilePath);                
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
    event->accept();
}