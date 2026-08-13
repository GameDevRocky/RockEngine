#include "engine/components/AudioListener.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "Engine.hpp"

AudioListener* AudioListener::GetMain()
{
    Container* container = Engine::Get()->GetActiveContainer();
    if (!container) return nullptr;
    SceneManager* sm = container->FindSystem<SceneManager>();
    if (!sm) return nullptr;

    for (Scene* scene : sm->GetScenes())
    {
        if (!scene) continue;
        for (GameObject* obj : scene->GetAllGameObjects())
        {
            if (!obj || !obj->GetActive()) continue;
            AudioListener* listener = obj->GetComponent<AudioListener>();
            if (listener && listener->GetEnabled()) return listener;
        }
    }
    return nullptr;
}

AudioListener* AudioListener::Copy()
{
    AudioListener* copy = new AudioListener();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    return copy;
}

YAML::Node AudioListener::Serialize()
{
    return Component::Serialize();
}

void AudioListener::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    state = State::Loaded;
}

void AudioListener::Accept(IVisitor* v)
{
    v->Visit(this);
}
