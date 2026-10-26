#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "engine/rendering/core/Resource.hpp"
#include "engine/tilemap/TileTypes.hpp"

struct TileType {
    std::string name;
    std::string defaultSpriteId;
    std::map<NeighborMask, std::string> autoTileRules;
};

class TileSet : public Resource {
public:
    static inline const Event CHANGED_EVENT = TileSet::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileSet"; }

    const std::unordered_map<Tile, TileType>& GetTileTypes() const {
        return tileTypes;
    }
    const TileType* GetTile(Tile id) const;

    bool SetTile(Tile id, TileType type);
    bool RemoveTile(Tile id);

    std::string ResolveSprite(TileTypeId id, const NeighborMask& neighbors) const;

private:
    std::unordered_map<TileTypeId, TileType> tileTypes;
};
