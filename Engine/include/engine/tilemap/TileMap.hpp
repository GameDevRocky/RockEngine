#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "engine/rendering/core/Resource.hpp"
#include "engine/tilemap/Tile.hpp"

struct TileCellHash {
    std::size_t operator()(const glm::ivec2& cell) const noexcept;
};

class TileMap : public Resource {

public:
    static inline const Event CELL_CHANGED_EVENT    = TileMap::CreateEvent();
    static inline const Event CLEARED_EVENT         = TileMap::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    TileMap* Copy() override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileMap"; }

    bool Load();
    void Unload();
    void MarkSaved() { dirty = false; if (loadCount == 0) cells.reset(); }
    bool IsDirty() const { return dirty; }

    bool ClearTile(const glm::ivec2& cell);
    bool Clear();
    
    bool SetTileId(const glm::ivec2& cell, TileId tileId);
    TileId GetTileId(const glm::ivec2& cell) const;
    bool HasTile(const glm::ivec2& cell) const;
    using CellData = std::unordered_map<glm::ivec2, TileId, TileCellHash>;
    // Null while the file-backed cell data is unloaded.
    const CellData* GetCells() const { return cells.get(); }

private:
    std::unique_ptr<CellData> cells;
    std::size_t loadCount = 0;
    bool dirty = false;
};
