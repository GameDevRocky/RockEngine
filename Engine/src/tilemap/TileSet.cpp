#include "engine/tilemap/TileSet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace {
const char* CollisionTypeName(TileCollisionType value)
{
    switch (value) {
    case TileCollisionType::Sprite: return "Sprite";
    case TileCollisionType::Grid: return "Grid";
    case TileCollisionType::Custom: return "Custom";
    default: return "None";
    }
}

TileCollisionType ParseCollisionType(const YAML::Node& node, TileCollisionType fallback)
{
    if (!node) return fallback;
    const std::string value = node.as<std::string>("None");
    if (value == "Sprite") return TileCollisionType::Sprite;
    if (value == "Grid" || value == "FullCell") return TileCollisionType::Grid;
    if (value == "Custom") return TileCollisionType::Custom;
    return TileCollisionType::None;
}

YAML::Node SerializePolygon(const TileCollisionPolygon& points)
{
    YAML::Node node(YAML::NodeType::Sequence);
    for (const glm::vec2& point : points) {
        YAML::Node pair(YAML::NodeType::Sequence);
        pair.push_back(point.x);
        pair.push_back(point.y);
        pair.SetStyle(YAML::EmitterStyle::Flow);
        node.push_back(pair);
    }
    return node;
}

TileCollisionPolygon ParsePolygon(const YAML::Node& node)
{
    if (!node || !node.IsSequence()) return DefaultTileCollisionPolygon();
    TileCollisionPolygon points;
    if (node.size() < 3 || node.size() > 8) return DefaultTileCollisionPolygon();
    try {
        for (const YAML::Node& pair : node) {
            if (!pair.IsSequence() || pair.size() != 2) return DefaultTileCollisionPolygon();
            const float x = pair[0].as<float>();
            const float y = pair[1].as<float>();
            points.emplace_back(std::clamp(x, -0.5f, 0.5f),
                                std::clamp(y, -0.5f, 0.5f));
        }
    } catch (const YAML::Exception&) {
        return DefaultTileCollisionPolygon();
    }
    return IsValidTileCollisionPolygon(points) ? points : DefaultTileCollisionPolygon();
}

const char* OutputTypeName(TileOutputType value)
{
    switch (value) {
    case TileOutputType::Random: return "Random";
    case TileOutputType::Animation: return "Animation";
    default: return "Fixed";
    }
}

TileOutputType ParseOutputType(const YAML::Node& node)
{
    const std::string value = node.as<std::string>("Fixed");
    if (value == "Random") return TileOutputType::Random;
    if (value == "Animation") return TileOutputType::Animation;
    return TileOutputType::Fixed;
}

std::uint32_t CellHash(const glm::ivec2& cell, TileId tileId)
{
    std::uint32_t value = static_cast<std::uint32_t>(cell.x) * 0x8da6b343u;
    value ^= static_cast<std::uint32_t>(cell.y) * 0xd8163841u;
    value ^= static_cast<std::uint32_t>(tileId) * 0xcb1ab31fu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return value;
}
}

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
        tileNode["collisionType"] = CollisionTypeName(tile.collisionType);
        if (tile.collisionType == TileCollisionType::Custom ||
            tile.customPolygon != DefaultTileCollisionPolygon())
            tileNode["customPolygon"] = SerializePolygon(tile.customPolygon);

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
            rule["collisionType"] = CollisionTypeName(tileRule.collisionType);
            if (tileRule.collisionType == TileCollisionType::Custom ||
                tileRule.customPolygon != DefaultTileCollisionPolygon())
                rule["customPolygon"] = SerializePolygon(tileRule.customPolygon);
            rule["output"] = OutputTypeName(tileRule.outputType);
            if (tileRule.outputType == TileOutputType::Fixed) {
                rule["spriteId"] = tileRule.spriteId;
            } else {
                YAML::Node sprites(YAML::NodeType::Sequence);
                for (const std::string& spriteId : tileRule.spriteIds) sprites.push_back(spriteId);
                rule["sprites"] = sprites;
            }
            if (tileRule.outputType == TileOutputType::Animation) {
                rule["minSpeed"] = tileRule.minAnimationSpeed;
                rule["maxSpeed"] = tileRule.maxAnimationSpeed;
            }
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
        tile.collisionType = ParseCollisionType(tileNode["collisionType"], TileCollisionType::None);
        tile.customPolygon = ParsePolygon(tileNode["customPolygon"]);

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

                AutoTileRule parsedRule;
                parsedRule.neighbors = neighbors;
                parsedRule.spriteId = rule["spriteId"].as<std::string>("");
                parsedRule.collisionType = ParseCollisionType(rule["collisionType"], tile.collisionType);
                parsedRule.customPolygon = ParsePolygon(rule["customPolygon"]);
                parsedRule.outputType = ParseOutputType(rule["output"]);
                if (const YAML::Node sprites = rule["sprites"]; sprites && sprites.IsSequence())
                    for (const YAML::Node& sprite : sprites)
                        parsedRule.spriteIds.push_back(sprite.as<std::string>(""));
                parsedRule.minAnimationSpeed = std::max(0.01f, rule["minSpeed"].as<float>(1.0f));
                parsedRule.maxAnimationSpeed = std::max(0.01f, rule["maxSpeed"].as<float>(1.0f));
                auto existing = std::find_if(tile.autoTileRules.begin(), tile.autoTileRules.end(),
                    [&](const AutoTileRule& candidate) { return candidate.neighbors == neighbors; });
                if (existing == tile.autoTileRules.end())
                    tile.autoTileRules.push_back(std::move(parsedRule));
                else
                    *existing = std::move(parsedRule);
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
    if (!IsValidTileCollisionPolygon(tile.customPolygon)) return false;
    for (const AutoTileRule& rule : tile.autoTileRules)
        if (!IsValidTileCollisionPolygon(rule.customPolygon)) return false;
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
    return ResolveTile(id, neighbors).spriteId;
}

ResolvedTile TileSet::ResolveTile(TileId id, const NeighborMask& neighbors,
                                  const glm::ivec2& cell, float animationTime) const
{
    const Tile* tile = GetTile(id);
    if (!tile) return {};

    const AutoTileRule* bestRule = nullptr;
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
            bestRule = &rule;
            bestSpecificity = specificity;
        }
    }
    if (!bestRule) return {tile->defaultSpriteId, tile->collisionType, TileOutputType::Fixed,
                          tile->collisionType == TileCollisionType::Custom ? &tile->customPolygon : nullptr};

    std::string spriteId = bestRule->spriteId;
    if (bestRule->outputType != TileOutputType::Fixed && !bestRule->spriteIds.empty()) {
        const std::uint32_t hash = CellHash(cell, id);
        if (bestRule->outputType == TileOutputType::Random) {
            spriteId = bestRule->spriteIds[hash % bestRule->spriteIds.size()];
        } else {
            const float low = std::min(bestRule->minAnimationSpeed, bestRule->maxAnimationSpeed);
            const float high = std::max(bestRule->minAnimationSpeed, bestRule->maxAnimationSpeed);
            const float fraction = static_cast<float>(hash & 0xffffu) / 65535.0f;
            const float speed = std::max(0.01f, low + (high - low) * fraction);
            const auto frame = static_cast<std::size_t>(std::floor(std::max(0.0f, animationTime) * speed));
            spriteId = bestRule->spriteIds[frame % bestRule->spriteIds.size()];
        }
    }
    if (spriteId.empty()) spriteId = tile->defaultSpriteId;
    return {spriteId, bestRule->collisionType, bestRule->outputType,
            bestRule->collisionType == TileCollisionType::Custom ? &bestRule->customPolygon : nullptr};
}
