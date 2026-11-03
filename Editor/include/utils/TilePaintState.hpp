#pragma once
#include "engine/tilemap/Tile.hpp"
#include <string>

struct TilePaintState {
    static TilePaintState& Get() { static TilePaintState state; return state; }
    std::string componentId;
    TileId selectedTileId = EMPTY_TILE_ID;
    int brushSize = 1;
};
