#include "engine/tilemap/TileSet.hpp"

#include <algorithm>
#include <utility>
#include <vector>

YAML::Node TileSet::Serialize()
{
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();

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
        for (const AutoTileRule& tileRule : tile.autoTileRules)
        {
            YAML::Node rule;
            YAML::Node neighborNode(YAML::NodeType::Sequence);
            for (const auto& neighbor : tileRule.neighbors) {
                if (neighbor) neighborNode.push_back(*neighbor);
                else neighborNode.push_back("any");
            }
            neighborNode.SetStyle(YAML::EmitterStyle::Flow);
            rule["neighbors"] = neighborNode;
            rule["spriteId"] = tileRule.spriteId;
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

                NeighborPattern neighbors{};
                for (std::size_t i = 0; i < neighbors.size(); ++i)
                {
                    const YAML::Node value = neighborNode[i];
                    if (value.IsScalar() && value.Scalar() == "any")
                        neighbors[i] = std::nullopt;
                    else
                        neighbors[i] = value.as<TileId>(EMPTY_TILE_ID);
                }

                const std::string spriteId = rule["spriteId"].as<std::string>("");
                auto existing = std::find_if(tile.autoTileRules.begin(), tile.autoTileRules.end(),
                    [&](const AutoTileRule& candidate) { return candidate.neighbors == neighbors; });
                if (existing == tile.autoTileRules.end())
                    tile.autoTileRules.push_back({neighbors, spriteId});
                else
                    existing->spriteId = spriteId;
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

    const std::string* bestSprite = nullptr;
    int bestSpecificity = -1;
    for (const AutoTileRule& rule : tile->autoTileRules)
    {
        bool matches = true;
        int specificity = 0;
        for (std::size_t i = 0; i < neighbors.size(); ++i)
        {
            if (!rule.neighbors[i]) continue;
            if (*rule.neighbors[i] != neighbors[i]) {
                matches = false;
                break;
            }
            ++specificity;
        }
        if (matches && specificity > bestSpecificity) {
            bestSprite = &rule.spriteId;
            bestSpecificity = specificity;
        }
    }
    return bestSprite ? *bestSprite : tile->defaultSpriteId;
}
