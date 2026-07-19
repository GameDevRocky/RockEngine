#include "engine/serialization/IdRemapper.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <utility>
#include <vector>

namespace {
    // Mint a fresh id for every node's "id" field in `seq`, skipping ids already
    // mapped (a node can legitimately appear in both sequences only by mistake,
    // but the guard keeps the mapping one-to-one either way).
    void CollectIds(const YAML::Node& seq, IdRemapper::IdMap& map) {
        if (!seq.IsSequence()) return;
        for (const auto& node : seq) {
            if (!node.IsMap() || !node["id"]) continue;
            const std::string oldId = node["id"].as<std::string>();
            if (oldId.empty() || map.count(oldId)) continue;
            map[oldId] = EngineUtils::GenerateUUID();
        }
    }
}

IdRemapper::IdMap IdRemapper::BuildMap(const YAML::Node& gameobjects,
                                       const YAML::Node& components)
{
    IdMap map;
    CollectIds(gameobjects, map);
    CollectIds(components, map);
    return map;
}

void IdRemapper::Apply(YAML::Node node, const IdMap& map)
{
    if (node.IsSequence()) {
        for (std::size_t i = 0; i < node.size(); ++i) {
            YAML::Node element = node[i];
            if (element.IsScalar()) {
                auto it = map.find(element.Scalar());
                if (it != map.end()) node[i] = it->second;
            } else {
                Apply(element, map);
            }
        }
        return;
    }

    if (node.IsMap()) {
        // Collect first, rewrite after. Mutating a yaml-cpp map while iterating
        // it is asking for trouble, and this runs once per duplicate — not per
        // frame — so the extra vector costs nothing that matters.
        std::vector<std::pair<std::string, std::string>> rewrites;
        for (auto entry : node) {
            YAML::Node value = entry.second;
            if (value.IsScalar()) {
                auto it = map.find(value.Scalar());
                if (it != map.end())
                    rewrites.emplace_back(entry.first.Scalar(), it->second);
            } else {
                Apply(value, map);
            }
        }
        for (const auto& [key, newValue] : rewrites)
            node[key] = newValue;
    }
}
