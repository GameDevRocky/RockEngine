#pragma once
#include "engine/components/Component.hpp"
#include "engine/components/AnimatorTypes.hpp"
#include "yaml-cpp/yaml.h"
#include <string>
#include <vector>

class Sprite;
class IVisitor;

// Unity-style sprite animation state machine. Holds named STATES (each a
// self-contained sprite flipbook), TRANSITIONS between them (guarded by
// conditions and/or exit time), and typed PARAMETERS that scripts set to drive
// those transitions. The whole machine is embedded on the component (serialized
// inline with the scene, deep-copied for play mode). In play mode Update() walks
// the machine and drives the GameObject's SpriteRenderer; the node-graph editor
// (AnimatorGui) authors it.
class Animator : public Component{

    public:
    static inline const Event STATE_CHANGED_EVENT      = Animator::CreateEvent();  // runtime current state switched
    static inline const Event GRAPH_CHANGED_EVENT       = Animator::CreateEvent();  // states/transitions edited
    static inline const Event PARAMETERS_CHANGED_EVENT   = Animator::CreateEvent();  // parameter list edited

    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override;
    void PostInit() override{};
    void Update() override;

    Animator* Copy() override;
    void Accept(IVisitor* v) override;

    // ─── Scripting / runtime control (also exposed to Python) ───────────────────
    void  SetFloat(const std::string& name, float v);
    void  SetInt(const std::string& name, int v);
    void  SetBool(const std::string& name, bool v);
    void  SetTrigger(const std::string& name);
    void  ResetTrigger(const std::string& name);
    float GetFloat(const std::string& name) const;
    int   GetInt(const std::string& name) const;
    bool  GetBool(const std::string& name) const;
    const std::string& GetCurrentState() const { return currentState; }
    void  Play(const std::string& stateName);   // force-enter a state now

    // ─── Editor mutators (fire GRAPH_CHANGED_EVENT / PARAMETERS_CHANGED_EVENT) ───
    AnimatorState*      AddState(const std::string& name, glm::vec2 pos = {0.0f, 0.0f});
    void                RemoveState(const std::string& name);
    void                RenameState(const std::string& oldName, const std::string& newName);
    AnimatorState*      FindState(const std::string& name);

    AnimatorTransition* AddTransition(const std::string& fromState, const std::string& toState, bool fromAny = false);
    void                RemoveTransition(const std::string& id);
    AnimatorTransition* FindTransition(const std::string& id);

    AnimatorParameter*  AddParameter(const std::string& name, AnimatorParameter::Type type);
    void                RemoveParameter(const std::string& name);
    void                RenameParameter(const std::string& oldName, const std::string& newName);
    AnimatorParameter*  FindParameter(const std::string& name);

    void SetDefaultState(const std::string& name);
    const std::string& GetDefaultState() const { return defaultState; }

    void AddFrame(const std::string& stateName, const std::string& spriteId);
    void RemoveFrame(const std::string& stateName, int frameIndex);
    // Reassign one slot in place. An empty spriteId is allowed and left as a hole,
    // so clearing a frame's picker doesn't silently shuffle every later frame.
    void SetFrame(const std::string& stateName, int frameIndex, const std::string& spriteId);
    // Reorder within the state; out-of-range indices are ignored.
    void MoveFrame(const std::string& stateName, int fromIndex, int toIndex);

    // Read-only views for the editor.
    const std::vector<AnimatorState>&      GetStates()      const { return states; }
    const std::vector<AnimatorTransition>& GetTransitions() const { return transitions; }
    const std::vector<AnimatorParameter>&  GetParameters()  const { return parameters; }

    // Editors mutate a struct in place (via a Find*), then call this so the graph
    // canvas / panels refresh.
    void NotifyGraphChanged() { Notify(GRAPH_CHANGED_EVENT); }
    void NotifyParametersChanged() { Notify(PARAMETERS_CHANGED_EVENT); }

    std::string GetTypeName() const override {return "Animator";}

private:
    void ApplyCurrentFrame();                             // push current frame's sprite to the SpriteRenderer
    void EnterState(const std::string& name);             // switch + reset timers + apply frame 0 + notify
    const AnimatorState* CurrentStateData() const;
    const AnimatorParameter* FindParameterConst(const std::string& name) const;
    bool EvaluateCondition(const AnimatorCondition& c) const;
    bool ConditionsMet(const AnimatorTransition& t) const;
    std::string MakeUniqueStateName(const std::string& base) const;
    std::string MakeUniqueParameterName(const std::string& base) const;

    // Definition (serialized).
    std::vector<AnimatorState>      states;
    std::vector<AnimatorTransition> transitions;
    std::vector<AnimatorParameter>  parameters;
    std::string defaultState;          // entry state name

    // Runtime (copied on the play-mode deep copy; re-seeded in Awake).
    std::string currentState;
    int   curr_index = 0;              // current frame within currentState
    float stateTime = 0.0f;            // seconds elapsed in currentState (for exit time)
    float timeAccumulator = 0.0f;      // frame timing; transient, not serialized
};
