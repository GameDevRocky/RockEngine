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

    treeView = new SceneTree(this);
    layout->addWidget(treeView);
    setAcceptDrops(true);
}

HierarchyGui::~HierarchyGui() {
    ClearTransformSubscriptions();
}

void HierarchyGui::Init(){ 
    std::cout << "HierarchyGui Initialized" << std::endl;
}

void HierarchyGui::PostInit(){
    std::cout << "Hierarchy Post Initialized" << std::endl;
    RequestHierarchyRefresh();
}

Scene* HierarchyGui::GetActiveScene() const {
    Engine* engine = Engine::Get();
    if (!engine || !engine->GetActiveContainer()) return nullptr;

    SceneManager* manager = engine->GetActiveContainer()->FindSystem<SceneManager>();
    if (!manager) return nullptr;

    std::vector<Scene*> scenes = manager->GetScenes();
    for (auto it = scenes.rbegin(); it != scenes.rend(); ++it) {
        if (*it) return *it;
    }

    return nullptr;
}

void HierarchyGui::RequestHierarchyRefresh() {
    hierarchyDirty = true;
}

void HierarchyGui::ClearTransformSubscriptions() {
    for (auto& [_, pair] : transformSubscriptions) {
        Transform* transform = pair.first;
        Callback* callback = pair.second;
        if (transform && callback) {
            transform->Unsubscribe(callback);
        }
    }
    transformSubscriptions.clear();
}

void HierarchyGui::SyncTransformSubscriptions(Scene* scene) {
    if (!scene) {
        if (!transformSubscriptions.empty()) {
            ClearTransformSubscriptions();
            RequestHierarchyRefresh();
        }
        return;
    }

    std::unordered_set<std::string> activeTransformIds;
    for (GameObject* object : scene->GetAllGameObjects()) {
        if (!object) continue;
        Transform* transform = object->GetTransform();
        if (!transform) continue;

        const std::string& transformId = transform->GetID();
        activeTransformIds.insert(transformId);

        auto existing = transformSubscriptions.find(transformId);
        if (existing != transformSubscriptions.end()) {
            if (existing->second.first == transform) {
                continue;
            }

            Transform* oldTransform = existing->second.first;
            Callback* oldCallback = existing->second.second;
            if (oldTransform && oldCallback) {
                oldTransform->Unsubscribe(oldCallback);
            }
            transformSubscriptions.erase(existing);
        }

        Callback* cb = transform->Subscribe([this]() {
            RequestHierarchyRefresh();
        }, Transform::PARENT_CHANGED_EVENT);

        transformSubscriptions.emplace(transformId, std::make_pair(transform, cb));
        RequestHierarchyRefresh();
    }

    std::vector<std::string> staleIds;
    staleIds.reserve(transformSubscriptions.size());
    for (const auto& [transformId, _] : transformSubscriptions) {
        if (!activeTransformIds.contains(transformId)) {
            staleIds.push_back(transformId);
        }
    }

    for (const std::string& transformId : staleIds) {
        auto found = transformSubscriptions.find(transformId);
        if (found != transformSubscriptions.end()) {
            Transform* transform = found->second.first;
            Callback* cb = found->second.second;
            if (transform && cb) {
                transform->Unsubscribe(cb);
            }
            transformSubscriptions.erase(found);
        }
        RequestHierarchyRefresh();
    }
}

void HierarchyGui::UpdateHierarchy() {
    Engine* engine = Engine::Get();
    const Container* nextContainer = engine ? engine->GetActiveContainer() : nullptr;
    Scene* activeScene = GetActiveScene();
    const std::string nextSceneId = activeScene ? activeScene->GetID() : std::string();
    const Scene* nextScenePtr = activeScene;

    if (activeContainerPtr != nextContainer || activeScenePtr != nextScenePtr || activeSceneId != nextSceneId) {
        activeContainerPtr = nextContainer;
        activeScenePtr = nextScenePtr;
        activeSceneId = nextSceneId;
        ClearTransformSubscriptions();
        RequestHierarchyRefresh();
    }

    SyncTransformSubscriptions(activeScene);

    if (!hierarchyDirty) {
        return;
    }

    auto* sceneTree = dynamic_cast<SceneTree*>(treeView);
    if (sceneTree) {
        sceneTree->RebuildFromScene(activeScene);
    }

    hierarchyDirty = false;
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
                RequestHierarchyRefresh();
                
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