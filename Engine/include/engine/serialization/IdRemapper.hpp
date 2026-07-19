#pragma once
#include <string>
#include <unordered_map>
#include "yaml-cpp/yaml.h"

// Rewrites object ids inside a serialized object graph so it can be
// re-instantiated as a *new* set of objects rather than a same-identity copy.
//
// Why this exists: every cross-reference in the engine is an id string
// (`component_ids`, `gameobject`, `parent_id`, and ScriptComponent ref fields).
// `Copy()` deliberately preserves ids — it's built for the play-mode container
// swap, where identical ids across containers are the invariant. Duplication
// needs the opposite, so it round-trips through YAML and remaps here.
namespace IdRemapper {

    using IdMap = std::unordered_map<std::string, std::string>;

    // Build old -> fresh-UUID for the "id" of every node in both sequences.
    // Pass the GameObject *and* component sequences so component ids (which
    // GameObject::component_ids points at) are remapped in the same pass.
    IdMap BuildMap(const YAML::Node& gameobjects, const YAML::Node& components);

    // Recursively rewrite every scalar *value* that appears as a key in `map`.
    //
    // Deliberately blunt: rather than enumerating which fields hold ids per
    // component type, it rewrites any string that matches. That is safe because
    // ids are 32-hex UUIDs and `map` only ever contains ids of objects inside
    // the duplicated set — so asset ids (material/sprite/shader/texture) and
    // references pointing *outside* the set are never matched, and are left
    // alone. It also means ScriptComponent's `fields:` refs are handled without
    // needing their refTypeName, which isn't known until IntrospectFields()
    // runs inside Init() — long after these values are loaded.
    //
    // Map *keys* are field names, never ids, and are not touched. Pass a
    // sequence or map — scalars are rewritten through their parent, so a bare
    // scalar at the root is a no-op.
    void Apply(YAML::Node node, const IdMap& map);
}
