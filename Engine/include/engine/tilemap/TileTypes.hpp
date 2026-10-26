#pragma once

#include <array>
#include <cstdint>

using Tile = std::uint16_t;

inline constexpr Tile EMPTY_TILE_TYPE = 0;
using NeighborMask = std::array<Tile, 8>;
