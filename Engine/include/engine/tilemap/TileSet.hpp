#pragma once

#include <string>
#include <unordered_map>

#include "engine/rendering/core/Resource.hpp"
#include "engine/tilemap/Tile.hpp"

class TileSet : public Resource {
public:
    static inline const Event CHANGED_EVENT = TileSet::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileSet"; }

    const std::unordered_map<TileId, Tile>& GetTiles() const {
        return tiles;
    }
    const Tile* GetTile(TileId id) const;

    bool SetTile(TileId id, Tile tile);
    bool RemoveTile(TileId id);

    std::string ResolveSprite(TileId id, const NeighborMask& neighbors) const;

private:
    std::unordered_map<TileId, Tile> tiles;
};
