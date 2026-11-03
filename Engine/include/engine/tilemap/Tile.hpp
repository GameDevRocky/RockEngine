#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using TileId = std::uint16_t;

inline constexpr TileId EMPTY_TILE_ID = 0;
using NeighborMask = std::array<TileId, 8>;
using NeighborPattern = std::array<std::optional<TileId>, 8>;

struct AutoTileRule {
    NeighborPattern neighbors{};
    std::string spriteId;
    bool operator==(const AutoTileRule&) const = default;
};

struct Tile {
    std::string name;
    std::string defaultSpriteId;
    std::vector<AutoTileRule> autoTileRules;
};
