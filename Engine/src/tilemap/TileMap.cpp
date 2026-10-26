#include "engine/tilemap/TileMap.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

std::size_t TileCellHash::operator()(const glm::ivec2& cell) const noexcept
{
    std::size_t seed = std::hash<int>{}(cell.x);
    seed ^= std::hash<int>{}(cell.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

YAML::Node TileMap::Serialize()
{
    const bool loadForSerialization = !tiles && !GetFilePath().empty();
    if (loadForSerialization && !Load())
        throw std::runtime_error("Failed to load TileMap cells: " + GetFilePath());

    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();
    YAML::Node cells(YAML::NodeType::Sequence);
    if (tiles)
    {
        std::vector<std::pair<glm::ivec2, TileTypeId>> orderedTiles;
        orderedTiles.reserve(tiles->size());
        for (const auto& entry : *tiles) orderedTiles.push_back(entry);
        std::sort(orderedTiles.begin(), orderedTiles.end(), [](const auto& a, const auto& b) {
            if (a.first.y != b.first.y) return a.first.y < b.first.y;
            return a.first.x < b.first.x;
        });

        for (const auto& [cell, type] : orderedTiles)
        {
            YAML::Node entry;
            entry["x"] = cell.x;
            entry["y"] = cell.y;
            entry["tileType"] = type;
            cells.push_back(entry);
        }
    }
    node["cells"] = cells;
    if (loadForSerialization) Unload();
    return node;
}

void TileMap::Deserialize(const YAML::Node& node)
{
    Resource::Deserialize(node);
    tiles.reset();
    loadCount = 0;
    dirty = false;
}

TileMap* TileMap::Copy()
{
    auto* copy = new TileMap();
    copy->id = id;
    copy->name = name;
    copy->m_filePath = m_filePath;
    if (tiles) copy->tiles = std::make_unique<data>(*tiles);
    copy->dirty = dirty;
    return copy;
}

void TileMap::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

bool TileMap::Load()
{
    if (tiles)
    {
        ++loadCount;
        return true;
    }

    if (GetFilePath().empty()) return false;

    try
    {
        const YAML::Node node = YAML::LoadFile(GetFilePath());
        const YAML::Node cells = node["cells"];
        if (!cells || !cells.IsSequence()) return false;

        auto loaded = std::make_unique<data>();
        for (const YAML::Node& entry : cells)
        {
            if (!entry["x"] || !entry["y"] || !entry["tileType"]) continue;
            const TileTypeId type = entry["tileType"].as<TileTypeId>(EMPTY_TILE_TYPE);
            if (type == EMPTY_TILE_TYPE) continue;
            (*loaded)[{entry["x"].as<int>(), entry["y"].as<int>()}] = type;
        }
        tiles = std::move(loaded);
        loadCount = 1;
        dirty = false;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void TileMap::Unload()
{
    if (loadCount > 0) --loadCount;
    if (loadCount == 0 && !dirty) tiles.reset();
}

bool TileMap::SetTileType(const glm::ivec2& cell, TileTypeId type)
{
    if (!tiles) return false;
    if (type == EMPTY_TILE_TYPE)
    {
        return ClearTile(cell);
    }

    const auto found = tiles->find(cell);
    if (found != tiles->end() && found->second == type) return false;
    (*tiles)[cell] = type;
    dirty = true;
    Notify(CELL_CHANGED_EVENT, cell);
    return true;
}

bool TileMap::ClearTile(const glm::ivec2& cell)
{
    if (!tiles || tiles->erase(cell) == 0) return false;
    dirty = true;
    Notify(CELL_CHANGED_EVENT, cell);
    return true;
}

bool TileMap::Clear()
{
    if (!tiles || tiles->empty()) return false;
    tiles->clear();
    dirty = true;
    Notify(CLEARED_EVENT);
    return true;
}

TileTypeId TileMap::GetTileType(const glm::ivec2& cell) const
{
    if (!tiles) return EMPTY_TILE_TYPE;
    const auto found = tiles->find(cell);
    return found == tiles->end() ? EMPTY_TILE_TYPE : found->second;
}

bool TileMap::HasTile(const glm::ivec2& cell) const
{
    return tiles && tiles->contains(cell);
}
