#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "engine/rendering/core/Resource.hpp"
#include "engine/tilemap/TileTypes.hpp"

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

    bool ClearTile(const glm::ivec2& cell);
    bool Clear();
    
    bool SetTile(const glm::ivec2& cell, Tile tile);
    Tile GetTile(const glm::ivec2& cell) const;
    bool HasTile(const glm::ivec2& cell) const;
    const std::unordered_map<glm::ivec2, Tile, TileCellHash>& GetTiles() { return tiles; }

private:
    std::unordered_map<glm::ivec2, Tile, TileCellHash> tiles;
    std::size_t loadCount = 0;
    bool dirty = false;
};
