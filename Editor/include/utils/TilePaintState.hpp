#pragma once
#include "engine/tilemap/Tile.hpp"
#include <string>

struct TilePaintState {
    enum class Tool { Select, Paint, Erase };
    static TilePaintState& Get() { static TilePaintState state; return state; }
    std::string componentId;
    TileId selectedTileId = EMPTY_TILE_ID;
    Tool tool = Tool::Select;
};
