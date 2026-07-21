#include "engine/components/Animator.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/utils/IVisitor.hpp"
#include "engine/debug/Console.hpp"


void Animator::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);   // id, enabled, gameobject_id

    currentAnimation = node["currentAnimation"].as<std::string>("");
    frameRate        = node["frameRate"].as<float>(12.0f);
    loop             = node["loop"].as<bool>(true);
    playing          = node["playing"].as<bool>(true);

    animations.clear();
    if (node["animations"] && node["animations"].IsMap()) {
        for (const auto& entry : node["animations"]) {
            std::vector<std::string> frames;
            if (entry.second.IsSequence())
                for (const auto& f : entry.second) frames.push_back(f.as<std::string>());
            animations[entry.first.as<std::string>()] = std::move(frames);
        }
    }

    state = State::Loaded;
}


YAML::Node Animator::Serialize(){
    YAML::Node node = Component::Serialize();   // id, enabled, type, gameobject
    node["currentAnimation"] = currentAnimation;
    node["frameRate"]        = frameRate;
    node["loop"]             = loop;
    node["playing"]          = playing;

    YAML::Node anims(YAML::NodeType::Map);
    for (const auto& [name, frames] : animations) {
        YAML::Node frameSeq(YAML::NodeType::Sequence);
        for (const auto& s : frames) frameSeq.push_back(s);
        frameSeq.SetStyle(YAML::EmitterStyle::Flow);
        anims[name] = frameSeq;
    }
    node["animations"] = anims;
    return node;
}


Animator* Animator::Copy(){
    Animator* copy = new Animator();
    copy->id               = id;
    copy->enabled          = enabled;
    copy->gameobject_id    = gameobject_id;
    copy->animations       = animations;
    copy->currentAnimation = currentAnimation;
    copy->curr_index       = curr_index;
    copy->frameRate        = frameRate;
    copy->loop             = loop;
    copy->playing          = playing;
    return copy;
}


void Animator::Accept(IVisitor* v){
    v->Visit(this);
}


void Animator::Awake(){
    // Play mode starts here: rewind and show the first frame so the animation
    // begins from the top. In edit mode we leave the SpriteRenderer alone.
    if (!container || container->GetMode() != Container::Mode::Runtime) return;

    curr_index = 0;
    timeAccumulator = 0.0f;
    auto it = animations.find(currentAnimation);
    if (it != animations.end() && !it->second.empty())
        ApplyCurrentFrame(it->second);
}


void Animator::Update(){
    // Only animate while the game is running -- in edit mode the SpriteRenderer
    // keeps whatever sprite the user assigned.
    if (!container || container->GetMode() != Container::Mode::Runtime) return;
    if (!playing || currentAnimation.empty() || frameRate <= 0.0f) return;

    auto it = animations.find(currentAnimation);
    if (it == animations.end() || it->second.empty()) return;
    const std::vector<std::string>& frames = it->second;

    TimeManager* tm = container->FindSystem<TimeManager>();
    const float dt = tm ? tm->DeltaTime() : 0.0f;

    // Clamp if the animation shrank since we last ran (e.g. edited live).
    bool frameChanged = false;
    if (curr_index >= static_cast<int>(frames.size())) { curr_index = 0; frameChanged = true; }

    timeAccumulator += dt;
    const float frameDuration = 1.0f / frameRate;
    while (timeAccumulator >= frameDuration) {
        timeAccumulator -= frameDuration;
        int next = curr_index + 1;
        if (next >= static_cast<int>(frames.size())) {
            if (loop) {
                next = 0;
            } else {
                next = static_cast<int>(frames.size()) - 1;
                playing = false;
                timeAccumulator = 0.0f;
            }
        }
        if (next != curr_index) { curr_index = next; frameChanged = true; }
        if (!playing) break;
    }

    // Only touch the SpriteRenderer when the frame actually advances, so we don't
    // fire SPRITE_CHANGED_EVENT every tick.
    if (frameChanged) ApplyCurrentFrame(frames);
}


void Animator::ApplyCurrentFrame(const std::vector<std::string>& frames){
    if (curr_index < 0 || curr_index >= static_cast<int>(frames.size())) return;
    SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
    if (!renderer) return;
    std::string frameId = frames[curr_index];   // SetSprite takes a non-const ref
    renderer->SetSprite(frameId);
}


void Animator::Play(const std::string& animation){
    if (!animations.contains(animation)){
        Console::Alert("Animator: no animation named '" + animation + "'.");
        return;
    }
    currentAnimation = animation;
    curr_index = 0;
    timeAccumulator = 0.0f;
    playing = true;

    auto it = animations.find(currentAnimation);
    if (it != animations.end() && !it->second.empty()) ApplyCurrentFrame(it->second);

    Notify(CURRENT_ANIMATION_CHANGED_EVENT, currentAnimation);
}


void Animator::AddAnimation(const std::string& animation ){
    if (!animations.contains(animation)){
        animations[animation] = std::vector<std::string>();
    }
    // The first animation added becomes the current one, so there is always a
    // meaningful "current animation" to show/play once any exist.
    if (currentAnimation.empty()){
        currentAnimation = animation;
        Notify(CURRENT_ANIMATION_CHANGED_EVENT, currentAnimation);
    }
    Notify(ANIMATION_ADDED_EVENT, animation);
}


void Animator::AddSprite(const std::string& animation, const std::string& sprite_id){
    if (!animations.contains(animation)){
        Console::Alert("Trying to add " + sprite_id + " to " + animation + " but animation does not exist." );
        return;
    }
    Sprite* sprite = AssetManager::Get().GetSprite(sprite_id);
    if (!sprite){
        Console::Alert("Trying to add " + sprite_id + " to " + animation + " but sprite does not exist." );
        return;
    }
    animations[animation].push_back(sprite_id);
    Notify(SPRITE_ADDED_EVENT, sprite_id);
}
