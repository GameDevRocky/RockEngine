#include "engine/components/Animator.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/utils/IVisitor.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"
#include <algorithm>
#include <cmath>


// ─── Serialization ──────────────────────────────────────────────────────────

void Animator::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);   // id, enabled, gameobject_id
    defaultState = node["defaultState"].as<std::string>("");

    states.clear();
    if (node["states"] && node["states"].IsSequence())
        for (const auto& sn : node["states"]) { AnimatorState s; s.Deserialize(sn); states.push_back(std::move(s)); }

    transitions.clear();
    if (node["transitions"] && node["transitions"].IsSequence())
        for (const auto& tn : node["transitions"]) { AnimatorTransition t; t.Deserialize(tn); transitions.push_back(std::move(t)); }

    parameters.clear();
    if (node["parameters"] && node["parameters"].IsSequence())
        for (const auto& pn : node["parameters"]) { AnimatorParameter p; p.Deserialize(pn); parameters.push_back(std::move(p)); }

    state = State::Loaded;
}

YAML::Node Animator::Serialize(){
    YAML::Node node = Component::Serialize();   // id, enabled, type, gameobject
    node["defaultState"] = defaultState;

    YAML::Node st(YAML::NodeType::Sequence);
    for (const auto& s : states) st.push_back(s.Serialize());
    node["states"] = st;

    YAML::Node tr(YAML::NodeType::Sequence);
    for (const auto& t : transitions) tr.push_back(t.Serialize());
    node["transitions"] = tr;

    YAML::Node pr(YAML::NodeType::Sequence);
    for (const auto& p : parameters) pr.push_back(p.Serialize());
    node["parameters"] = pr;

    return node;
}

Animator* Animator::Copy(){
    Animator* c = new Animator();
    c->id            = id;
    c->enabled       = enabled;
    c->gameobject_id = gameobject_id;
    c->states        = states;
    c->transitions   = transitions;
    c->parameters    = parameters;
    c->defaultState  = defaultState;
    c->currentState  = currentState;
    c->curr_index    = curr_index;
    c->stateTime     = stateTime;
    return c;
}

void Animator::Accept(IVisitor* v){
    v->Visit(this);
}


// ─── Runtime ────────────────────────────────────────────────────────────────

void Animator::Awake(){
    // Play mode starts here: seed parameter values from their defaults, enter the
    // default state, and show its first frame. In edit mode we leave everything be.
    if (!container || container->GetMode() != Container::Mode::Runtime) return;

    for (auto& p : parameters) p.value = p.defaultValue;

    std::string start = !defaultState.empty() ? defaultState
                        : (states.empty() ? std::string() : states.front().name);
    currentState.clear();
    if (!start.empty()) EnterState(start);
}

void Animator::Update(){
    if (!container || container->GetMode() != Container::Mode::Runtime) return;
    if (currentState.empty()) return;

    const AnimatorState* s = CurrentStateData();

    // Advance the current state's flipbook.
    if (s && !s->frames.empty() && s->frameRate > 0.0f) {
        TimeManager* tm = container->FindSystem<TimeManager>();
        const float dt = tm ? tm->DeltaTime() : 0.0f;
        stateTime += dt;
        timeAccumulator += dt * s->speed;

        const float frameDuration = 1.0f / s->frameRate;
        bool frameChanged = false;
        if (curr_index >= static_cast<int>(s->frames.size())) { curr_index = 0; frameChanged = true; }
        while (timeAccumulator >= frameDuration) {
            timeAccumulator -= frameDuration;
            int next = curr_index + 1;
            if (next >= static_cast<int>(s->frames.size())) {
                if (s->loop) next = 0;
                else { next = static_cast<int>(s->frames.size()) - 1; timeAccumulator = 0.0f; }
            }
            if (next != curr_index) { curr_index = next; frameChanged = true; }
        }
        if (frameChanged) ApplyCurrentFrame();
    }

    // Evaluate transitions: first match wins (one switch per frame).
    for (const auto& t : transitions) {
        if (!t.fromAnyState && t.fromState != currentState) continue;
        if (t.toState.empty() || t.toState == currentState) continue;

        // Exit time gate: only allow once the clip has played past the fraction.
        if (t.hasExitTime && s && !s->frames.empty() && s->frameRate > 0.0f) {
            const float clipLen = s->frames.size() / s->frameRate;
            if (clipLen > 0.0f && (stateTime / clipLen) < t.exitTime) continue;
        }
        if (!ConditionsMet(t)) continue;

        // Consume any Trigger parameters this transition matched on.
        for (const auto& c : t.conditions)
            if (AnimatorParameter* p = FindParameter(c.parameter);
                p && p->type == AnimatorParameter::Type::Trigger)
                p->value = 0.0f;

        EnterState(t.toState);
        break;
    }
}

void Animator::EnterState(const std::string& name){
    currentState = name;
    curr_index = 0;
    stateTime = 0.0f;
    timeAccumulator = 0.0f;
    ApplyCurrentFrame();
    Notify(STATE_CHANGED_EVENT, currentState);
}

void Animator::ApplyCurrentFrame(){
    const AnimatorState* s = CurrentStateData();
    if (!s || s->frames.empty()) return;
    if (curr_index < 0 || curr_index >= static_cast<int>(s->frames.size())) return;
    SpriteRenderer* renderer = GetComponent<SpriteRenderer>();
    if (!renderer) return;
    std::string frameId = s->frames[curr_index];   // SetSprite takes a non-const ref
    renderer->SetSprite(frameId);
}

const AnimatorState* Animator::CurrentStateData() const {
    for (const auto& s : states) if (s.name == currentState) return &s;
    return nullptr;
}

bool Animator::EvaluateCondition(const AnimatorCondition& c) const {
    const AnimatorParameter* p = FindParameterConst(c.parameter);
    if (!p) return false;
    switch (c.mode) {
        case AnimatorCondition::Mode::Greater:   return p->value >  c.threshold;
        case AnimatorCondition::Mode::Less:      return p->value <  c.threshold;
        case AnimatorCondition::Mode::Equals:    return p->value == c.threshold;
        case AnimatorCondition::Mode::NotEquals: return p->value != c.threshold;
        case AnimatorCondition::Mode::If:        return p->value != 0.0f;
        case AnimatorCondition::Mode::IfNot:     return p->value == 0.0f;
    }
    return false;
}

bool Animator::ConditionsMet(const AnimatorTransition& t) const {
    for (const auto& c : t.conditions)
        if (!EvaluateCondition(c)) return false;
    return true;   // no conditions -> met (fires immediately, or on exit time if set)
}


// ─── Scripting / runtime control ──────────────────────────────────────────────

void Animator::SetFloat(const std::string& name, float v)  { if (auto* p = FindParameter(name)) p->value = v; }
void Animator::SetInt(const std::string& name, int v)      { if (auto* p = FindParameter(name)) p->value = static_cast<float>(v); }
void Animator::SetBool(const std::string& name, bool v)    { if (auto* p = FindParameter(name)) p->value = v ? 1.0f : 0.0f; }
void Animator::SetTrigger(const std::string& name)         { if (auto* p = FindParameter(name)) p->value = 1.0f; }
void Animator::ResetTrigger(const std::string& name)       { if (auto* p = FindParameter(name)) p->value = 0.0f; }

float Animator::GetFloat(const std::string& name) const { auto* p = FindParameterConst(name); return p ? p->value : 0.0f; }
int   Animator::GetInt(const std::string& name) const   { auto* p = FindParameterConst(name); return p ? static_cast<int>(std::lround(p->value)) : 0; }
bool  Animator::GetBool(const std::string& name) const  { auto* p = FindParameterConst(name); return p ? (p->value != 0.0f) : false; }

void Animator::Play(const std::string& stateName){
    if (!FindState(stateName)) {
        Console::Alert("Animator: no state named '" + stateName + "'.");
        return;
    }
    EnterState(stateName);
}


// ─── Lookup helpers ───────────────────────────────────────────────────────────

AnimatorState* Animator::FindState(const std::string& name){
    for (auto& s : states) if (s.name == name) return &s;
    return nullptr;
}
AnimatorTransition* Animator::FindTransition(const std::string& id){
    for (auto& t : transitions) if (t.id == id) return &t;
    return nullptr;
}
AnimatorParameter* Animator::FindParameter(const std::string& name){
    for (auto& p : parameters) if (p.name == name) return &p;
    return nullptr;
}
const AnimatorParameter* Animator::FindParameterConst(const std::string& name) const {
    for (auto& p : parameters) if (p.name == name) return &p;
    return nullptr;
}

std::string Animator::MakeUniqueStateName(const std::string& base) const {
    auto taken = [&](const std::string& n){ for (auto& s : states) if (s.name == n) return true; return false; };
    if (!taken(base)) return base;
    for (int i = 1; ; ++i) { std::string cand = base + " " + std::to_string(i); if (!taken(cand)) return cand; }
}
std::string Animator::MakeUniqueParameterName(const std::string& base) const {
    auto taken = [&](const std::string& n){ for (auto& p : parameters) if (p.name == n) return true; return false; };
    if (!taken(base)) return base;
    for (int i = 1; ; ++i) { std::string cand = base + " " + std::to_string(i); if (!taken(cand)) return cand; }
}


// ─── Editor mutators ──────────────────────────────────────────────────────────

AnimatorState* Animator::AddState(const std::string& name, glm::vec2 pos){
    AnimatorState s;
    s.name = MakeUniqueStateName(name.empty() ? "New State" : name);
    s.editorPos = pos;
    states.push_back(std::move(s));
    if (defaultState.empty()) defaultState = states.back().name;   // first state becomes the entry
    Notify(GRAPH_CHANGED_EVENT);
    return &states.back();
}

void Animator::RemoveState(const std::string& name){
    transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
        [&](const AnimatorTransition& t){ return t.fromState == name || t.toState == name; }), transitions.end());
    states.erase(std::remove_if(states.begin(), states.end(),
        [&](const AnimatorState& s){ return s.name == name; }), states.end());
    if (defaultState == name) defaultState = states.empty() ? std::string() : states.front().name;
    if (currentState == name) currentState.clear();
    Notify(GRAPH_CHANGED_EVENT);
}

void Animator::RenameState(const std::string& oldName, const std::string& newName){
    if (newName.empty() || oldName == newName || FindState(newName)) return;   // reject empty/duplicate
    if (auto* s = FindState(oldName)) s->name = newName; else return;
    for (auto& t : transitions) {
        if (t.fromState == oldName) t.fromState = newName;
        if (t.toState == oldName)   t.toState = newName;
    }
    if (defaultState == oldName)  defaultState = newName;
    if (currentState == oldName)  currentState = newName;
    Notify(GRAPH_CHANGED_EVENT);
}

AnimatorTransition* Animator::AddTransition(const std::string& fromState, const std::string& toState, bool fromAny){
    AnimatorTransition t;
    t.id = EngineUtils::GenerateUUID();
    t.fromAnyState = fromAny;
    t.fromState = fromAny ? std::string() : fromState;
    t.toState = toState;
    transitions.push_back(std::move(t));
    Notify(GRAPH_CHANGED_EVENT);
    return &transitions.back();
}

void Animator::RemoveTransition(const std::string& id){
    transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
        [&](const AnimatorTransition& t){ return t.id == id; }), transitions.end());
    Notify(GRAPH_CHANGED_EVENT);
}

AnimatorParameter* Animator::AddParameter(const std::string& name, AnimatorParameter::Type type){
    AnimatorParameter p;
    p.name = MakeUniqueParameterName(name.empty() ? "New Parameter" : name);
    p.type = type;
    parameters.push_back(std::move(p));
    Notify(PARAMETERS_CHANGED_EVENT);
    return &parameters.back();
}

void Animator::RemoveParameter(const std::string& name){
    parameters.erase(std::remove_if(parameters.begin(), parameters.end(),
        [&](const AnimatorParameter& p){ return p.name == name; }), parameters.end());
    // Drop conditions that referenced it so transitions don't dangle.
    for (auto& t : transitions)
        t.conditions.erase(std::remove_if(t.conditions.begin(), t.conditions.end(),
            [&](const AnimatorCondition& c){ return c.parameter == name; }), t.conditions.end());
    Notify(PARAMETERS_CHANGED_EVENT);
}

void Animator::RenameParameter(const std::string& oldName, const std::string& newName){
    if (newName.empty() || oldName == newName || FindParameter(newName)) return;   // reject empty/duplicate
    if (auto* p = FindParameter(oldName)) p->name = newName; else return;
    for (auto& t : transitions)
        for (auto& c : t.conditions)
            if (c.parameter == oldName) c.parameter = newName;
    Notify(PARAMETERS_CHANGED_EVENT);
}

void Animator::SetDefaultState(const std::string& name){
    if (!FindState(name)) return;
    defaultState = name;
    Notify(GRAPH_CHANGED_EVENT);
}

void Animator::AddFrame(const std::string& stateName, const std::string& spriteId){
    if (auto* s = FindState(stateName)) { s->frames.push_back(spriteId); Notify(GRAPH_CHANGED_EVENT); }
}

void Animator::RemoveFrame(const std::string& stateName, int frameIndex){
    if (auto* s = FindState(stateName))
        if (frameIndex >= 0 && frameIndex < static_cast<int>(s->frames.size())) {
            s->frames.erase(s->frames.begin() + frameIndex);
            Notify(GRAPH_CHANGED_EVENT);
        }
}
