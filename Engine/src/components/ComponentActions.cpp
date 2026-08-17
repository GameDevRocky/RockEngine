#include "engine/components/ComponentActions.hpp"

#include "engine/components/Component.hpp"
#include "engine/components/AudioSource.hpp"
#include "engine/components/Animator.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/ScriptComponent.hpp"

#include <charconv>
#include <cstdlib>
#include <algorithm>

namespace {

ComponentActionInfo MakeAction(const char* name, const char* label,
                               const char* tooltip = "") {
    ComponentActionInfo a;
    a.name = name;
    a.label = label;
    a.tooltip = tooltip;
    return a;
}

ComponentActionInfo MakeAction(const char* name, const char* label,
                               const char* tooltip,
                               const char* argName, const char* argType) {
    ComponentActionInfo a = MakeAction(name, label, tooltip);
    a.hasArg = true;
    a.arg.name = argName;
    a.arg.typeName = argType;
    return a;
}

// Trim, then parse. Every parser returns false rather than throwing, because an
// argument string comes from a serialized scene or an MCP client and being
// malformed is an ordinary outcome, not an exceptional one.
std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

bool ParseFloatArg(const std::string& raw, float& out) {
    const std::string t = Trim(raw);
    if (t.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<float>(v);
    return true;
}

bool ParseIntArg(const std::string& raw, int& out) {
    const std::string t = Trim(raw);
    if (t.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (end == t.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

}  // namespace

namespace ComponentActions {

std::vector<ComponentActionInfo> For(Component* component) {
    std::vector<ComponentActionInfo> actions;
    if (!component) return actions;

    if (dynamic_cast<AudioSource*>(component)) {
        actions.push_back(MakeAction("play",    "Play"));
        actions.push_back(MakeAction("stop",    "Stop"));
        actions.push_back(MakeAction("pause",   "Pause"));
        actions.push_back(MakeAction("unpause", "Unpause"));
        actions.push_back(MakeAction("play_one_shot", "Play One Shot",
                                     "Play the clip without interrupting the current playback.",
                                     "volume_scale", "float"));
    } else if (dynamic_cast<Animator*>(component)) {
        actions.push_back(MakeAction("play", "Play State",
                                     "Force-enter a state by name.", "state", "str"));
        actions.push_back(MakeAction("set_trigger",   "Set Trigger",   "", "name", "str"));
        actions.push_back(MakeAction("reset_trigger", "Reset Trigger", "", "name", "str"));
    } else if (dynamic_cast<ParticleComponent*>(component)) {
        actions.push_back(MakeAction("emit_burst", "Emit Burst", "", "count", "int"));
    } else if (auto* script = dynamic_cast<ScriptComponent*>(component)) {
        // A script's actions come from its Python class (@action), so they change
        // with every hot-reload rather than being fixed per type.
        for (const ScriptActionInfo& a : script->GetScriptActions()) {
            ComponentActionInfo info;
            info.name = a.name;
            info.label = a.label;
            info.tooltip = a.tooltip;
            info.hasArg = a.hasArg;
            info.arg.name = a.argName;
            info.arg.typeName = a.argTypeName;
            info.arg.refTypeName = a.argRefTypeName;
            actions.push_back(std::move(info));
        }
    }
    return actions;
}

bool Invoke(Component* component, const std::string& action,
            const std::string& rawArg, std::string* error) {
    if (!component) return Fail(error, "no component");

    // Scripts first: a ScriptComponent's action list is dynamic, so there is
    // nothing to match against here -- hand the name to Python and let the
    // instance answer.
    if (auto* script = dynamic_cast<ScriptComponent*>(component))
        return script->InvokeAction(action, rawArg, error);

    if (auto* audio = dynamic_cast<AudioSource*>(component)) {
        if (action == "play")    { audio->Play();    return true; }
        if (action == "stop")    { audio->Stop();    return true; }
        if (action == "pause")   { audio->Pause();   return true; }
        if (action == "unpause") { audio->UnPause(); return true; }
        if (action == "play_one_shot") {
            float scale = 1.0f;
            if (!Trim(rawArg).empty() && !ParseFloatArg(rawArg, scale))
                return Fail(error, "volume_scale must be a number");
            audio->PlayOneShot(scale);
            return true;
        }
    } else if (auto* animator = dynamic_cast<Animator*>(component)) {
        const std::string name = Trim(rawArg);
        if (action == "play")           { animator->Play(name);         return true; }
        if (action == "set_trigger")    { animator->SetTrigger(name);   return true; }
        if (action == "reset_trigger")  { animator->ResetTrigger(name); return true; }
    } else if (auto* particles = dynamic_cast<ParticleComponent*>(component)) {
        if (action == "emit_burst") {
            int count = 0;
            if (!ParseIntArg(rawArg, count) || count < 1)
                return Fail(error, "count must be a positive integer");
            particles->EmitBurst(count);
            return true;
        }
    }

    return Fail(error, "unsupported action \"" + action + "\" for " +
                       component->GetTypeName());
}

}  // namespace ComponentActions
