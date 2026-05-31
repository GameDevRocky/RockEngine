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
    
    transform->Subscribe([tree, gameObjectId](const std::any& data) {
        
        std::string newParentTransformId = std::any_cast<std::string>(data);
        
        std::string newParentGameObjectId;
        if (!newParentTransformId.empty()) {
            Transform* parentTransform = Registry::FindInRuntime<Transform>(newParentTransformId);
            if (parentTransform && parentTransform->GetGameObject()) {
                newParentGameObjectId = parentTransform->GetGameObject()->GetID();
            }
        }
        
        tree->ReparentItem(gameObjectId, newParentGameObjectId);
        return true;
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

    // Inner widget that holds the scene trees
    scrollWidget = new QWidget();
    scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(0);
    scrollLayout->addStretch(1);

    // Scroll area wrapping the inner widget
    scrollArea = new QScrollArea(this);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(4, 4, 4, 4);
    outerLayout->addWidget(scrollArea);
    setLayout(outerLayout);
    setAcceptDrops(true);
}

HierarchyGui::~HierarchyGui() {

}

void HierarchyGui::Init(){ 
    std::cout << "HierarchyGui Initialized" << std::endl;
}

void HierarchyGui::PostInit(){
    std::cout << "Hierarchy Post Initialized" << std::endl;
    sceneManager->Subscribe([this](const std::any& data){
        const std::string& id = std::any_cast<std::string>(data);
        this->AddSceneTree(id);
        return true;
    }, SceneManager::LOADED_SCENE_EVENT);

    sceneManager->Subscribe([this](const std::any& data){
        const std::string& id = std::any_cast<std::string>(data);
        this->RemoveSceneTree(id);
        return true;
    }, SceneManager::REMOVED_SCENE_EVENT);

    Engine::Get()->Subscribe([this](){
        this->RefreshHierarchy();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);

    Engine::Get()->Subscribe([this](){
        this->RefreshHierarchy();
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);
}


void HierarchyGui::AddSceneTree(const std::string& scene_id) {
    Scene* scene = registry->Find<Scene>(scene_id);
    
    SceneTree* tree = new SceneTree();
    std::string sceneId = scene->GetID();
    sceneTrees[sceneId] = tree;
    tree->RebuildFromScene(scene);
    tree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    SubscribeToSceneTransforms(scene, tree);
    // Insert before the trailing stretch
    int insertIndex = scrollLayout->count() - 1;
    scrollLayout->insertWidget(insertIndex, tree);
}



void HierarchyGui::RemoveSceneTree(const std::string& id) {
    auto it = sceneTrees.find(id);
    if (it == sceneTrees.end()) return;

    SceneTree* tree = it->second;
    scrollLayout->removeWidget(tree);
    tree->deleteLater();
    sceneTrees.erase(it);
    RefreshHierarchy();
}

void HierarchyGui::RefreshHierarchy() {
    const auto& scenes = sceneManager->GetScenes();

    std::unordered_set<std::string> currentSceneIds;
    for (auto* scene : scenes) {
        currentSceneIds.insert(scene->GetID());
    }

    for (auto it = sceneTrees.begin(); it != sceneTrees.end(); ) {
        if (currentSceneIds.find(it->first) == currentSceneIds.end()) {
            it->second->deleteLater();
            it = sceneTrees.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* scene : scenes) {
        std::string sceneId = scene->GetID();
        if (sceneTrees.count(sceneId)) {
            sceneTrees[sceneId]->RebuildFromScene(scene);
            SubscribeToSceneTransforms(scene, sceneTrees[sceneId]);
        } else {
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
    setStyleSheet("");
    
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
            
        QString filePath = url.toLocalFile();
        
        if (!filePath.endsWith(".yaml", Qt::CaseInsensitive) && 
            !filePath.endsWith(".yml", Qt::CaseInsensitive))
            continue;
        
        std::string sceneFilePath = filePath.toStdString();
        Console::Comment("Loading scene from: " + sceneFilePath);
        sceneManager->LoadScene(sceneFilePath);                
        event->acceptProposedAction();
        return;
          
    event->ignore();
    }
}

void HierarchyGui::dragLeaveEvent(QDragLeaveEvent* event) {
    event->accept();
}