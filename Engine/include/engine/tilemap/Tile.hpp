#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

using TileId = std::uint16_t;

inline constexpr TileId EMPTY_TILE_ID = 0;
using NeighborMask = std::array<TileId, 8>;

struct Tile {
    std::string name;
    std::string defaultSpriteId;
    std::map<NeighborMask, std::string> autoTileRules;
};
