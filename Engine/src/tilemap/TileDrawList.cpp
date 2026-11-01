#include "engine/tilemap/TileDrawList.hpp"
#include "engine/tilemap/TileRender.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include <algorithm>

std::vector<VisibleTile> TileDrawList::CollectVisible(TileMapEditor* editor,
                                                         TileMapRenderer* renderer,
                                                         const glm::mat4& viewProjection,
                                                         std::string* firstIssue) {
    std::vector<VisibleTile> visible;
    if (firstIssue) firstIssue->clear();
    if (!editor || !renderer || !editor->GetGrid()) return visible;
    TileMap* map = editor->GetTileMap();
    TileSet* set = editor->GetTileSet();
    if (!map || !map->GetCells() || !set) return visible;

    for (const auto& [cell, tileId] : *map->GetCells()) {
        const Tile* tile = set->GetTile(tileId);
        if (!tile) {
            if (firstIssue && firstIssue->empty())
                *firstIssue = "TileMap '" + map->GetName() + "' contains tile ID "
                    + std::to_string(tileId) + " with no definition in TileSet '"
                    + set->GetName() + "'";
            continue;
        }
        Sprite* sprite = TileRender::Resolve(editor, cell, tileId);
        if (!sprite) {
            if (firstIssue && firstIssue->empty())
                *firstIssue = "TileSet '" + set->GetName() + "' tile ID "
                    + std::to_string(tileId) + " has no usable sprite";
            continue;
        }
        Texture2D* texture = sprite->GetTexture();
        if (!texture || !texture->GetTextureID()) {
            if (firstIssue && firstIssue->empty())
                *firstIssue = "TileSet '" + set->GetName() + "' tile ID "
                    + std::to_string(tileId) + " has no loaded texture";
            continue;
        }
        const glm::vec2 size = TileRender::Size(editor, renderer, sprite);
        if (size.x <= 0.0f || size.y <= 0.0f) continue;
        const glm::mat4 model = TileRender::Model(editor, cell, size);
        if (!TileRender::Visible(model, size, sprite->GetPivot(), viewProjection)) continue;
        visible.push_back({cell, tileId, sprite, texture, size, model});
    }

    std::sort(visible.begin(), visible.end(), [](const VisibleTile& a, const VisibleTile& b) {
        return a.cell.y != b.cell.y ? a.cell.y > b.cell.y : a.cell.x < b.cell.x;
    });
    return visible;
}
