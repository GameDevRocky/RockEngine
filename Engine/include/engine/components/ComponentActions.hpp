#pragma once

#include <string>
#include <vector>

class Component;

// One invokable method on a component.
//
// There is exactly one registry of these, and three consumers read it: the
// Inspector's action buttons, the method dropdown inside an event call entry,
// and MCP's component.invoke. Before this existed the Inspector hand-wrote a
// QPushButton per visitor and MCP kept a parallel hardcoded dynamic_cast chain,
// with nothing keeping the two in agreement.
struct ComponentActionArg {
    std::string name;
    // Same vocabulary as ScriptFieldInfo::typeName, so the editor can build an
    // argument widget with the machinery it already has:
    // "float" | "int" | "bool" | "str" | "vec2" | "vec3" | "vec4"
    std::string typeName;
    // "sprite" | "material" | "gameobject:<Class>" | "component:<Type>", or empty
    // for a plain value. Only meaningful when typeName is "str".
    std::string refTypeName;
};

struct ComponentActionInfo {
    std::string name;      // the method to call
    std::string label;     // what the editor shows on the button / in the dropdown
    std::string tooltip;
    bool hasArg = false;   // at most one argument, by design — see Invoke()
    ComponentActionArg arg;
};

namespace ComponentActions {

// Every action `component` exposes. Native components have a fixed list; a
// ScriptComponent contributes whatever its Python class decorated with @action.
std::vector<ComponentActionInfo> For(Component* component);

// Call one. `rawArg` is the argument in its serialized string form and is
// ignored by actions that take none:
//   float/int/bool/str -> the literal ("1.5", "3", "true", "hello")
//   vec2/vec3/vec4     -> comma separated ("1,2" / "1,2,3")
//   refs               -> the target's id
//
// One argument rather than N is deliberate: it covers essentially all real
// event wiring, and it keeps a call entry serializable as a single flat string
// (see ScriptEvent's encoding) instead of needing a nested argument list.
//
// Returns false and fills `error` (when non-null) if the action is unknown or
// the argument cannot be parsed. Never throws.
bool Invoke(Component* component, const std::string& action,
            const std::string& rawArg, std::string* error = nullptr);

}  // namespace ComponentActions
