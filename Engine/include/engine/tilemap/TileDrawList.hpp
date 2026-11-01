#pragma once

#include "engine/tilemap/Tile.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Sprite;
class Texture2D;
class TileMapEditor;
class TileMapRenderer;

struct VisibleTile {
    glm::ivec2 cell{0};
    TileId tileId = EMPTY_TILE_ID;
    Sprite* sprite = nullptr;
    Texture2D* texture = nullptr;
    glm::vec2 size{0.0f};
    glm::mat4 model{1.0f};
};

// Resolves sparse TileMap cells into the same visible quads for color rendering
// and object picking. No GL or Qt state is owned here.
class TileDrawList {
public:
    static std::vector<VisibleTile> CollectVisible(TileMapEditor* editor,
                                                   TileMapRenderer* renderer,
                                                   const glm::mat4& viewProjection,
                                                   std::string* firstIssue = nullptr);
};
