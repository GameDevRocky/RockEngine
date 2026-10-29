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
    const bool loadForSerialization = !cells && !GetFilePath().empty();
    if (loadForSerialization && !Load())
        throw std::runtime_error("Failed to load TileMap cells: " + GetFilePath());

    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();
    YAML::Node serializedCells(YAML::NodeType::Sequence);
    if (cells)
    {
        std::vector<std::pair<glm::ivec2, TileId>> orderedTiles;
        orderedTiles.reserve(cells->size());
        for (const auto& entry : *cells) orderedTiles.push_back(entry);
        std::sort(orderedTiles.begin(), orderedTiles.end(), [](const auto& a, const auto& b) {
            if (a.first.y != b.first.y) return a.first.y < b.first.y;
            return a.first.x < b.first.x;
        });

        for (const auto& [cell, tileId] : orderedTiles)
        {
            YAML::Node entry;
            entry["x"] = cell.x;
            entry["y"] = cell.y;
            entry["tileId"] = tileId;
            serializedCells.push_back(entry);
        }
    }
    node["cells"] = serializedCells;
    if (loadForSerialization) Unload();
    return node;
}

void TileMap::Deserialize(const YAML::Node& node)
{
    Resource::Deserialize(node);
    cells.reset();
    loadCount = 0;
    dirty = false;
}

TileMap* TileMap::Copy()
{
    auto* copy = new TileMap();
    copy->id = id;
    copy->name = name;
    copy->m_filePath = m_filePath;
    if (cells) copy->cells = std::make_unique<CellData>(*cells);
    copy->dirty = dirty;
    return copy;
}

void TileMap::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

bool TileMap::Load()
{
    if (cells)
    {
        ++loadCount;
        return true;
    }

    if (GetFilePath().empty()) return false;

    try
    {
        const YAML::Node node = YAML::LoadFile(GetFilePath());
        const YAML::Node serializedCells = node["cells"];
        if (!serializedCells || !serializedCells.IsSequence()) return false;

        auto loaded = std::make_unique<CellData>();
        for (const YAML::Node& entry : serializedCells)
        {
            if (!entry["x"] || !entry["y"] || !entry["tileId"]) continue;
            const TileId tileId = entry["tileId"].as<TileId>(EMPTY_TILE_ID);
            if (tileId == EMPTY_TILE_ID) continue;
            (*loaded)[{entry["x"].as<int>(), entry["y"].as<int>()}] = tileId;
        }
        cells = std::move(loaded);
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
    if (loadCount == 0 && !dirty) cells.reset();
}

bool TileMap::SetTileId(const glm::ivec2& cell, TileId tileId)
{
    if (!cells) return false;
    if (tileId == EMPTY_TILE_ID)
    {
        return ClearTile(cell);
    }

    const auto found = cells->find(cell);
    if (found != cells->end() && found->second == tileId) return false;
    (*cells)[cell] = tileId;
    dirty = true;
    Notify(CELL_CHANGED_EVENT, cell);
    return true;
}

bool TileMap::ClearTile(const glm::ivec2& cell)
{
    if (!cells || cells->erase(cell) == 0) return false;
    dirty = true;
    Notify(CELL_CHANGED_EVENT, cell);
    return true;
}

bool TileMap::Clear()
{
    if (!cells || cells->empty()) return false;
    cells->clear();
    dirty = true;
    Notify(CLEARED_EVENT);
    return true;
}

TileId TileMap::GetTileId(const glm::ivec2& cell) const
{
    if (!cells) return EMPTY_TILE_ID;
    const auto found = cells->find(cell);
    return found == cells->end() ? EMPTY_TILE_ID : found->second;
}

bool TileMap::HasTile(const glm::ivec2& cell) const
{
    return cells && cells->contains(cell);
}
