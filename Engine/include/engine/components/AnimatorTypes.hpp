#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

// Plain data structures for the Animator's Unity-style state machine. Each has a
// Serialize()/Deserialize() so the Animator component can persist them inline (the
// whole machine lives on the component -- see Animator.hpp).

// A typed parameter that scripts set to drive transitions. One float carries every
// type (Int rounds, Bool/Trigger are 0/1) to keep set/get/serialize trivial.
// `value` is runtime state (reset from defaultValue in Animator::Awake); only
// name/type/defaultValue are serialized.
struct AnimatorParameter {
    enum class Type { Float, Int, Bool, Trigger };

    std::string name;
    Type  type = Type::Float;
    float value = 0.0f;          // runtime current value
    float defaultValue = 0.0f;

    YAML::Node Serialize() const {
        YAML::Node n;
        n["name"] = name;
        n["type"] = static_cast<int>(type);
        n["default"] = defaultValue;
        return n;
    }
    void Deserialize(const YAML::Node& n) {
        name = n["name"].as<std::string>("");
        type = static_cast<Type>(n["type"].as<int>(0));
        defaultValue = n["default"].as<float>(0.0f);
        value = defaultValue;
    }
};

// One clause of a transition: compare a parameter to a threshold. If/IfNot are the
// Bool/Trigger forms (true / false), the numeric modes are for Float/Int.
struct AnimatorCondition {
    enum class Mode { Greater, Less, Equals, NotEquals, If, IfNot };

    std::string parameter;
    Mode  mode = Mode::Greater;
    float threshold = 0.0f;

    YAML::Node Serialize() const {
        YAML::Node n;
        n["parameter"] = parameter;
        n["mode"] = static_cast<int>(mode);
        n["threshold"] = threshold;
        return n;
    }
    void Deserialize(const YAML::Node& n) {
        parameter = n["parameter"].as<std::string>("");
        mode = static_cast<Mode>(n["mode"].as<int>(0));
        threshold = n["threshold"].as<float>(0.0f);
    }
};

// A directed edge between states, taken when every condition passes (and, if
// hasExitTime, once the source clip has played past exitTime). fromAnyState edges
// can fire from whatever state is current (Unity's "Any State").
struct AnimatorTransition {
    std::string id;                         // stable unique id (editor selection + serialization)
    std::string fromState;                  // ignored when fromAnyState
    bool        fromAnyState = false;
    std::string toState;
    std::vector<AnimatorCondition> conditions;
    bool  hasExitTime = false;
    float exitTime = 1.0f;                   // normalized fraction of the source clip

    YAML::Node Serialize() const {
        YAML::Node n;
        n["id"] = id;
        n["from"] = fromState;
        n["fromAny"] = fromAnyState;
        n["to"] = toState;
        n["hasExitTime"] = hasExitTime;
        n["exitTime"] = exitTime;
        YAML::Node conds(YAML::NodeType::Sequence);
        for (const auto& c : conditions) conds.push_back(c.Serialize());
        n["conditions"] = conds;
        return n;
    }
    void Deserialize(const YAML::Node& n) {
        id = n["id"].as<std::string>("");
        fromState = n["from"].as<std::string>("");
        fromAnyState = n["fromAny"].as<bool>(false);
        toState = n["to"].as<std::string>("");
        hasExitTime = n["hasExitTime"].as<bool>(false);
        exitTime = n["exitTime"].as<float>(1.0f);
        conditions.clear();
        if (n["conditions"] && n["conditions"].IsSequence())
            for (const auto& cn : n["conditions"]) {
                AnimatorCondition c;
                c.Deserialize(cn);
                conditions.push_back(std::move(c));
            }
    }
};

// A state = a self-contained sprite flipbook (its own frames/fps/loop/speed) plus
// its node position in the graph editor.
struct AnimatorState {
    std::string name;
    std::vector<std::string> frames;   // AssetManager sprite ids, in play order
    float frameRate = 12.0f;           // frames per second
    float speed = 1.0f;                // playback speed multiplier
    bool  loop = true;
    glm::vec2 editorPos{0.0f, 0.0f};   // node position in the graph canvas

    YAML::Node Serialize() const {
        YAML::Node n;
        n["name"] = name;
        YAML::Node f(YAML::NodeType::Sequence);
        for (const auto& s : frames) f.push_back(s);
        f.SetStyle(YAML::EmitterStyle::Flow);
        n["frames"] = f;
        n["frameRate"] = frameRate;
        n["speed"] = speed;
        n["loop"] = loop;
        YAML::Node pos(YAML::NodeType::Sequence);
        pos.push_back(editorPos.x);
        pos.push_back(editorPos.y);
        pos.SetStyle(YAML::EmitterStyle::Flow);
        n["pos"] = pos;
        return n;
    }
    void Deserialize(const YAML::Node& n) {
        name = n["name"].as<std::string>("");
        frames.clear();
        if (n["frames"] && n["frames"].IsSequence())
            for (const auto& fn : n["frames"]) frames.push_back(fn.as<std::string>());
        frameRate = n["frameRate"].as<float>(12.0f);
        speed = n["speed"].as<float>(1.0f);
        loop = n["loop"].as<bool>(true);
        if (n["pos"] && n["pos"].IsSequence() && n["pos"].size() == 2)
            editorPos = glm::vec2(n["pos"][0].as<float>(0.0f), n["pos"][1].as<float>(0.0f));
    }
};
