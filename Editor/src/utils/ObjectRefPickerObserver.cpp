#include "utils/ObjectRefPickerObserver.hpp"

#include <algorithm>
#include <any>
#include <utility>

#include <QMetaObject>

#include "Engine.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Resource.hpp"

namespace {
bool IsAssetTag(Properties::Tags tag) {
    return tag == Properties::Tags::MATERIAL
        || tag == Properties::Tags::SPRITE
        || tag == Properties::Tags::TEXTURE
        || tag == Properties::Tags::SHADER
        || tag == Properties::Tags::FONT
        || tag == Properties::Tags::AUDIO_CLIP;
}
}

ObjectRefPickerObserver::ObjectRefPickerObserver(
    const Properties::PropDesc& desc, std::function<void()> refresh, QObject* parent)
    : QObject(parent), m_desc(desc), m_refresh(std::move(refresh)) {}

ObjectRefPickerObserver::~ObjectRefPickerObserver() {
    ClearAll();
}

void ObjectRefPickerObserver::Start() {
    if (m_active || m_desc.tag == Properties::Tags::SCRIPT) return;
    m_active = true;
    BindRoots();
    RebindDynamic();
}

void ObjectRefPickerObserver::Stop() {
    m_active = false;
    m_refreshQueued = false;
    m_rebindQueued = false;
    ClearAll();
}

void ObjectRefPickerObserver::BindRoots() {
    if (IsAssetTag(m_desc.tag)) {
        auto* assets = &AssetManager::Get();
        Subscribe(assets, AssetManager::ASSET_ADDED_EVENT,
            [this](std::any) {
                QueueRefresh(true);
                return true;
            });
        Subscribe(assets, AssetManager::ASSET_REMOVED_EVENT,
            [this](std::any data) {
                if (const auto* id = std::any_cast<std::string>(&data))
                    MarkAssetGone(*id);
                QueueRefresh(true);
                return true;
            });
        return;
    }

    auto* engine = Engine::Get();
    auto* container = engine ? engine->GetActiveContainer() : nullptr;
    auto* scenes = container ? container->FindSystem<SceneManager>() : nullptr;
    if (!scenes) return;

    Subscribe(scenes, SceneManager::LOADED_SCENE_EVENT,
        [this](std::any) {
            QueueRefresh(true);
            return true;
        });
    Subscribe(scenes, SceneManager::REMOVED_SCENE_EVENT,
        [this](std::any) {
            QueueRefresh(true);
            return true;
        });
    Subscribe(scenes, RuntimeObject::SHUTDOWN_EVENT,
        [this, scenes](std::any) {
            MarkSourceGone(scenes);
            QueueRefresh(true);
            return false;
        });
}

void ObjectRefPickerObserver::RebindDynamic() {
    if (!m_active) return;
    ClearDynamic();
    if (IsAssetTag(m_desc.tag)) ObserveAssets();
    else ObserveObjects();
}

void ObjectRefPickerObserver::ObserveAssets() {
    auto observeMap = [this](const auto& resources) {
        for (const auto& [id, resource] : resources) {
            if (!resource) continue;
            // ANY_EVENT also catches thumbnail-affecting resource edits. Name
            // changes are the common case, but rebuilding on another explicit
            // resource mutation keeps the open visual picker honest as well.
            Subscribe(resource, Observable::ANY_EVENT,
                [this](std::any) {
                    QueueRefresh(false);
                    return true;
                }, true, id);
        }
    };

    auto& assets = AssetManager::Get();
    switch (m_desc.tag) {
        case Properties::Tags::MATERIAL:   observeMap(assets.GetAllMaterials()); break;
        case Properties::Tags::SPRITE:     observeMap(assets.GetAllSprites()); break;
        case Properties::Tags::TEXTURE:    observeMap(assets.GetAllTextures()); break;
        case Properties::Tags::SHADER:     observeMap(assets.GetAllShaders()); break;
        case Properties::Tags::FONT:       observeMap(assets.GetAllFonts()); break;
        case Properties::Tags::AUDIO_CLIP: observeMap(assets.GetAllAudioClips()); break;
        default: break;
    }
}

void ObjectRefPickerObserver::ObserveObjects() {
    auto* engine = Engine::Get();
    auto* container = engine ? engine->GetActiveContainer() : nullptr;
    auto* sceneManager = container ? container->FindSystem<SceneManager>() : nullptr;
    if (!sceneManager) return;

    for (Scene* scene : sceneManager->GetScenes()) {
        if (!scene) continue;
        Subscribe(scene, Scene::GAMEOBJECT_ADDED_EVENT,
            [this](std::any) {
                QueueRefresh(true);
                return true;
            }, true);
        Subscribe(scene, RuntimeObject::SHUTDOWN_EVENT,
            [this, scene](std::any) {
                MarkSourceGone(scene);
                QueueRefresh(true);
                return false;
            }, true);

        for (GameObject* object : scene->GetAllGameObjects()) {
            if (!object || object->IsMarkedForDestroy()) continue;
            Subscribe(object, GameObject::NAME_CHANGED_EVENT,
                [this](std::any) {
                    QueueRefresh(false);
                    return true;
                }, true);
            Subscribe(object, GameObject::ADD_COMPONENT_EVENT,
                [this](std::any) {
                    QueueRefresh(true);
                    return true;
                }, true);
            Subscribe(object, GameObject::REMOVE_COMPONENT_EVENT,
                [this](std::any) {
                    QueueRefresh(true);
                    return true;
                }, true);
            Subscribe(object, RuntimeObject::SHUTDOWN_EVENT,
                [this, object](std::any) {
                    MarkSourceGone(object);
                    QueueRefresh(true);
                    return false;
                }, true);

            // Script reassignment can move an object into or out of a
            // gameobject:<Class> filtered field without adding a component.
            if (!m_desc.refClassFilter.empty()) {
                for (ScriptComponent* script : object->GetComponents<ScriptComponent>()) {
                    if (!script) continue;
                    Subscribe(script, ScriptComponent::SCRIPT_RELOADED_EVENT,
                        [this](std::any) {
                            QueueRefresh(false);
                            return true;
                        }, true);
                    Subscribe(script, RuntimeObject::SHUTDOWN_EVENT,
                        [this, script](std::any) {
                            MarkSourceGone(script);
                            QueueRefresh(true);
                            return false;
                        }, true);
                }
            }

            // GameObject cells preview their SpriteRenderer, so a sprite swap is
            // also an observable picker change even though membership is stable.
            if (auto* renderer = object->GetComponent<SpriteRenderer>()) {
                Subscribe(renderer, SpriteRenderer::SPRITE_CHANGED_EVENT,
                    [this](std::any) {
                        QueueRefresh(false);
                        return true;
                    }, true);
                Subscribe(renderer, RuntimeObject::SHUTDOWN_EVENT,
                    [this, renderer](std::any) {
                        MarkSourceGone(renderer);
                        QueueRefresh(true);
                        return false;
                    }, true);
            }
        }
    }
}

void ObjectRefPickerObserver::QueueRefresh(bool rebind) {
    if (!m_active) return;
    m_rebindQueued = m_rebindQueued || rebind;
    if (m_refreshQueued) return;
    m_refreshQueued = true;

    QMetaObject::invokeMethod(this, [this]() {
        if (!m_active) return;
        const bool rebind = m_rebindQueued;
        m_refreshQueued = false;
        m_rebindQueued = false;
        if (rebind) RebindDynamic();
        if (m_refresh) m_refresh();
    }, Qt::QueuedConnection);
}

void ObjectRefPickerObserver::MarkSourceGone(Observable* source) {
    for (auto& subscription : m_subscriptions)
        if (subscription.source == source) subscription.source = nullptr;
}

void ObjectRefPickerObserver::MarkAssetGone(const std::string& id) {
    for (auto& subscription : m_subscriptions)
        if (subscription.dynamic && subscription.key == id)
            subscription.source = nullptr;
}

void ObjectRefPickerObserver::ClearDynamic() {
    for (const auto& subscription : m_subscriptions)
        if (subscription.dynamic && subscription.source)
            subscription.source->Unsubscribe(subscription.id);
    std::erase_if(m_subscriptions,
        [](const Subscription& subscription) { return subscription.dynamic; });
}

void ObjectRefPickerObserver::ClearAll() {
    for (const auto& subscription : m_subscriptions)
        if (subscription.source) subscription.source->Unsubscribe(subscription.id);
    m_subscriptions.clear();
}

void ObjectRefPickerObserver::Subscribe(
    Observable* source, Observable::Event event, Observable::payload_function callback,
    bool dynamic, std::string key)
{
    if (!source) return;
    const int id = source->Subscribe(callback, event);
    m_subscriptions.push_back({source, id, dynamic, std::move(key)});
}
