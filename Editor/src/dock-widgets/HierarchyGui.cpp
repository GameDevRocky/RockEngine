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
#include "engine/serialization/Registry.hpp"
#include <unordered_set>

namespace {
void SubscribeTransformRecursive(Transform* transform, SceneTree* tree) {
    if (!transform) return;

    GameObject* gameObject = transform->GetGameObject();
    if (!gameObject) return;

    std::string gameObjectId = gameObject->GetID();
    
    transform->Subscribe([tree, gameObjectId](std::any data) {
        // Skip if the tree is handling a drop (UI already moved)
        if (tree->IsHandlingDrop()) return;
        
        std::string newParentTransformId = std::any_cast<std::string>(data);
        
        // Convert transform ID to gameobject ID
        std::string newParentGameObjectId;
        if (!newParentTransformId.empty()) {
            Transform* parentTransform = Registry::FindInRuntime<Transform>(newParentTransformId);
            if (parentTransform && parentTransform->GetGameObject()) {
                newParentGameObjectId = parentTransform->GetGameObject()->GetID();
            }
        }
        
        tree->ReparentItem(gameObjectId, newParentGameObjectId);
    }, Transform::PARENT_CHANGED_EVENT);

    for (Transform* child : transform->GetChildren()) {
        SubscribeTransformRecursive(child, tree);
    }
}

void SubscribeToSceneTransforms(Scene* scene, SceneTree* tree) {
    if (!scene || !tree) return;
    
    for (GameObject* rootObject : scene->GetRootObjects()) {
        Transform* transform = rootObject->GetTransform();
        SubscribeTransformRecursive(transform, tree);
    }
}
}

HierarchyGui::HierarchyGui(QWidget* parent) : QWidget(parent){
    setMinimumWidth(200);
    setMaximumWidth(400);
    layout = new QVBoxLayout();
    layout->setContentsMargins(4,4,4,4);
    layout->addStretch(1);
    
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
    sceneManager->Subscribe([this](const std::any& data){
        const std::string& id = std::any_cast<std::string>(data);
        this->AddSceneTree(id);
    }, SceneManager::LOADED_SCENE_EVENT);

    sceneManager->Subscribe([this](std::any data){
        const std::string& id = std::any_cast<std::string>(data);
        this->RemoveSceneTree(id);
    }, SceneManager::LOADED_SCENE_EVENT);

    Engine::Get()->Subscribe([this](){
        this->RefreshHierarchy();
    }, Engine::ENTER_PLAY_MODE_EVENT);

    Engine::Get()->Subscribe([this](){
        this->RefreshHierarchy();
    }, Engine::EXIT_PLAY_MODE_EVENT);
}


void HierarchyGui::AddSceneTree(const std::string& scene_id) {
    auto* registry = Engine::Get()->GetActiveContainer()->FindSystem<Registry>();
    Scene* scene = registry->Find<Scene>(scene_id);
    
    SceneTree* tree = new SceneTree();
    std::string sceneId = scene->GetID();
    sceneTrees[sceneId] = tree;
    tree->RebuildFromScene(scene);
    tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    SubscribeToSceneTransforms(scene, tree);
    int insertIndex = layout->count() - 1;
    layout->insertWidget(insertIndex, tree);
}



void HierarchyGui::RemoveSceneTree(const std::string& id) {
    
    
}

void HierarchyGui::RefreshHierarchy() {
    SceneManager* sceneManager = Engine::Get()->GetActiveContainer()->FindSystem<SceneManager>();
    const auto& scenes = sceneManager->GetScenes();

    // Build a set of current scene IDs
    std::unordered_set<std::string> currentSceneIds;
    for (auto* scene : scenes) {
        currentSceneIds.insert(scene->GetID());
    }

    // Remove trees that no longer have a matching scene
    for (auto it = sceneTrees.begin(); it != sceneTrees.end(); ) {
        if (currentSceneIds.find(it->first) == currentSceneIds.end()) {
            it->second->deleteLater();
            it = sceneTrees.erase(it);
        } else {
            ++it;
        }
    }

    // Rebuild remaining trees and add new ones
    for (auto* scene : scenes) {
        std::string sceneId = scene->GetID();
        if (sceneTrees.count(sceneId)) {
            // Tree exists — just rebuild it
            sceneTrees[sceneId]->RebuildFromScene(scene);
        } else {
            // New scene — add a new tree
            AddSceneTree(sceneId);
        }
    }
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