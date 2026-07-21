#pragma once
#include "engine/components/Component.hpp"
#include "yaml-cpp/yaml.h"
#include <string>
#include <vector>
#include <map>

class Sprite;
class IVisitor;

// Sprite-flipbook animator. Holds named animations (each an ordered list of
// sprite ids) and, in play mode, drives the GameObject's SpriteRenderer by
// swapping its sprite at `frameRate` FPS. The editor GUI for authoring these
// (a Unity-style graph) comes later; for now it stores/plays what's set on it.
class Animator : public Component{

    public:
    static inline const Event ANIMATION_ADDED_EVENT           = Animator::CreateEvent();
    static inline const Event SPRITE_ADDED_EVENT              = Animator::CreateEvent();
    static inline const Event CURRENT_ANIMATION_CHANGED_EVENT = Animator::CreateEvent();


    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override;
    void PostInit() override{};
    void Update() override;

    Animator* Copy() override;
    void Accept(IVisitor* v) override;

    void AddAnimation(const std::string& animation );
    void AddSprite(const std::string& animation, const std::string& sprite_id);

    // Switch the playing animation, restarting it at its first frame. No-op if
    // no animation by that name exists.
    void Play(const std::string& animation);

    const std::string& GetCurrentAnimation() const { return currentAnimation; }
    const std::map<std::string, std::vector<std::string>>& GetAnimations() const { return animations; }

    float GetFrameRate() const { return frameRate; }
    void  SetFrameRate(float fps) { frameRate = fps; }
    bool  IsPlaying() const { return playing; }
    void  SetPlaying(bool p) { playing = p; }
    bool  IsLooping() const { return loop; }
    void  SetLooping(bool l) { loop = l; }

    std::string GetTypeName() const override {return "Animator";}

private:
    // Push the sprite at curr_index of `frames` onto the GameObject's SpriteRenderer.
    void ApplyCurrentFrame(const std::vector<std::string>& frames);

    std::map<std::string, std::vector<std::string>>  animations;  // name -> ordered frame sprite ids
    std::string currentAnimation;      // "" when none is selected
    int   curr_index = 0;              // current frame within currentAnimation
    float frameRate = 12.0f;           // frames per second
    bool  loop = true;
    bool  playing = true;
    float timeAccumulator = 0.0f;      // runtime-only; not serialized

};
