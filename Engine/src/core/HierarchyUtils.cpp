#include "engine/core/HierarchyUtils.hpp"
#include <unordered_set>
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"

namespace {
// Cycle insurance. WouldCreateCycle in the editor prevents cycles being *created*,
// but a hand-edited or corrupt scene file could carry one, and an unbounded walk
// here would hang the editor rather than fail visibly.
constexpr int kMaxDepth = 256;
}

std::vector<std::string> HierarchyUtils::FilterToRoots(Container* container,
                                                       const std::vector<std::string>& ids)
{
    if (!container || ids.size() <= 1) return ids;

    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return ids;

    const std::unordered_set<std::string> candidates(ids.begin(), ids.end());

    std::vector<std::string> roots;
    roots.reserve(ids.size());

    for (const std::string& id : ids) {
        auto* object = registry->Find<GameObject>(id);
        Transform* transform = object ? object->GetTransform() : nullptr;
        if (!transform) {
            roots.push_back(id);   // asset or dead id — not ours to filter
            continue;
        }

        bool covered = false;
        int depth = 0;
        for (Transform* ancestor = transform->GetParent();
             ancestor && depth < kMaxDepth;
             ancestor = ancestor->GetParent(), ++depth) {
            GameObject* owner = ancestor->GetGameObject();
            if (owner && candidates.count(owner->GetID())) { covered = true; break; }
        }

        if (!covered) roots.push_back(id);
    }

    return roots;
}
