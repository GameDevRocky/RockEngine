#include "engine/tilemap/TileSet.hpp"

#include <algorithm>
#include <utility>
#include <vector>

YAML::Node TileSet::Serialize()
{
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();

    std::vector<TileTypeId> ids;
    ids.reserve(tileTypes.size());
    for (const auto& [id, _] : tileTypes) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    YAML::Node types(YAML::NodeType::Sequence);
    for (TileTypeId id : ids)
    {
        const TileType& type = tileTypes.at(id);
        YAML::Node typeNode;
        typeNode["id"] = id;
        typeNode["name"] = type.name;
        typeNode["defaultSpriteId"] = type.defaultSpriteId;

        YAML::Node rules(YAML::NodeType::Sequence);
        for (const auto& [neighbors, spriteId] : type.autoTileRules)
        {
            YAML::Node rule;
            YAML::Node neighborNode(YAML::NodeType::Sequence);
            for (TileTypeId neighbor : neighbors) neighborNode.push_back(neighbor);
            neighborNode.SetStyle(YAML::EmitterStyle::Flow);
            rule["neighbors"] = neighborNode;
            rule["spriteId"] = spriteId;
            rules.push_back(rule);
        }
        typeNode["rules"] = rules;
        types.push_back(typeNode);
    }
    node["tileTypes"] = types;
    return node;
}

void TileSet::Deserialize(const YAML::Node& node)
{
    Resource::Deserialize(node);
    tileTypes.clear();

    const YAML::Node types = node["tileTypes"];
    if (!types || !types.IsSequence()) return;

    for (const YAML::Node& typeNode : types)
    {
        if (!typeNode["id"]) continue;
        const TileTypeId id = typeNode["id"].as<TileTypeId>(EMPTY_TILE_TYPE);
        if (id == EMPTY_TILE_TYPE) continue;

        TileType type;
        type.name = typeNode["name"].as<std::string>("");
        type.defaultSpriteId = typeNode["defaultSpriteId"].as<std::string>("");

        const YAML::Node rules = typeNode["rules"];
        if (rules && rules.IsSequence())
        {
            for (const YAML::Node& rule : rules)
            {
                const YAML::Node neighborNode = rule["neighbors"];
                if (!neighborNode || !neighborNode.IsSequence() || neighborNode.size() != 8)
                    continue;

                NeighborMask neighbors{};
                for (std::size_t i = 0; i < neighbors.size(); ++i)
                    neighbors[i] = neighborNode[i].as<TileTypeId>(EMPTY_TILE_TYPE);

                const std::string spriteId = rule["spriteId"].as<std::string>("");
                type.autoTileRules[neighbors] = spriteId;
            }
        }

        tileTypes[id] = std::move(type);
    }
}

void TileSet::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

const TileType* TileSet::GetTileType(TileTypeId id) const
{
    const auto found = tileTypes.find(id);
    return found == tileTypes.end() ? nullptr : &found->second;
}

bool TileSet::SetTileType(TileTypeId id, TileType type)
{
    if (id == EMPTY_TILE_TYPE) return false;
    tileTypes[id] = std::move(type);
    Notify(CHANGED_EVENT, id);
    return true;
}

bool TileSet::RemoveTileType(TileTypeId id)
{
    if (id == EMPTY_TILE_TYPE || tileTypes.erase(id) == 0) return false;
    Notify(CHANGED_EVENT, id);
    return true;
}

std::string TileSet::ResolveSprite(TileTypeId id, const NeighborMask& neighbors) const
{
    const TileType* type = GetTileType(id);
    if (!type) return {};

    const auto rule = type->autoTileRules.find(neighbors);
    return rule == type->autoTileRules.end() ? type->defaultSpriteId : rule->second;
}
