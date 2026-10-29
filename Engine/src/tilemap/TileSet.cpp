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

    std::vector<TileId> ids;
    ids.reserve(tiles.size());
    for (const auto& [id, _] : tiles) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    YAML::Node serializedTiles(YAML::NodeType::Sequence);
    for (TileId id : ids)
    {
        const Tile& tile = tiles.at(id);
        YAML::Node tileNode;
        tileNode["id"] = id;
        tileNode["name"] = tile.name;
        tileNode["defaultSpriteId"] = tile.defaultSpriteId;

        YAML::Node rules(YAML::NodeType::Sequence);
        for (const auto& [neighbors, spriteId] : tile.autoTileRules)
        {
            YAML::Node rule;
            YAML::Node neighborNode(YAML::NodeType::Sequence);
            for (TileId neighbor : neighbors) neighborNode.push_back(neighbor);
            neighborNode.SetStyle(YAML::EmitterStyle::Flow);
            rule["neighbors"] = neighborNode;
            rule["spriteId"] = spriteId;
            rules.push_back(rule);
        }
        tileNode["rules"] = rules;
        serializedTiles.push_back(tileNode);
    }
    node["tiles"] = serializedTiles;
    return node;
}

void TileSet::Deserialize(const YAML::Node& node)
{
    Resource::Deserialize(node);
    tiles.clear();

    const YAML::Node serializedTiles = node["tiles"];
    if (!serializedTiles || !serializedTiles.IsSequence()) return;

    for (const YAML::Node& tileNode : serializedTiles)
    {
        if (!tileNode["id"]) continue;
        const TileId id = tileNode["id"].as<TileId>(EMPTY_TILE_ID);
        if (id == EMPTY_TILE_ID) continue;

        Tile tile;
        tile.name = tileNode["name"].as<std::string>("");
        tile.defaultSpriteId = tileNode["defaultSpriteId"].as<std::string>("");

        const YAML::Node rules = tileNode["rules"];
        if (rules && rules.IsSequence())
        {
            for (const YAML::Node& rule : rules)
            {
                const YAML::Node neighborNode = rule["neighbors"];
                if (!neighborNode || !neighborNode.IsSequence() || neighborNode.size() != 8)
                    continue;

                NeighborMask neighbors{};
                for (std::size_t i = 0; i < neighbors.size(); ++i)
                    neighbors[i] = neighborNode[i].as<TileId>(EMPTY_TILE_ID);

                const std::string spriteId = rule["spriteId"].as<std::string>("");
                tile.autoTileRules[neighbors] = spriteId;
            }
        }

        tiles[id] = std::move(tile);
    }
}

void TileSet::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

const Tile* TileSet::GetTile(TileId id) const
{
    const auto found = tiles.find(id);
    return found == tiles.end() ? nullptr : &found->second;
}

bool TileSet::SetTile(TileId id, Tile tile)
{
    if (id == EMPTY_TILE_ID) return false;
    tiles[id] = std::move(tile);
    Notify(CHANGED_EVENT, id);
    return true;
}

bool TileSet::RemoveTile(TileId id)
{
    if (id == EMPTY_TILE_ID || tiles.erase(id) == 0) return false;
    Notify(CHANGED_EVENT, id);
    return true;
}

std::string TileSet::ResolveSprite(TileId id, const NeighborMask& neighbors) const
{
    const Tile* tile = GetTile(id);
    if (!tile) return {};

    const auto rule = tile->autoTileRules.find(neighbors);
    return rule == tile->autoTileRules.end() ? tile->defaultSpriteId : rule->second;
}
