#pragma once

#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/tilemap/TileSet.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace TileRender {
inline glm::vec2 Size(TileMapEditor* editor, TileMapRenderer* renderer, Sprite* sprite) {
    return renderer->GetTileSizingMode() == TileMapRenderer::TileSizingMode::FillCell
        ? editor->GetGrid()->GetSize() : sprite->GetPixelSize();
}

// Sprite shaders use a centered quad and subtract uSize*uPivot. The extra half
// size places the sprite's authored pivot at the center of the Grid cell.
inline glm::mat4 Model(TileMapEditor* editor, const glm::ivec2& cell,
                       const glm::vec2& size) {
    Grid* grid = editor->GetGrid();
    const glm::vec2 origin = glm::vec2(cell) * (grid->GetSize() + grid->GetGap());
    return glm::translate(editor->GetTransform()->GetWorldMatrix(),
                          glm::vec3(origin + grid->GetSize() * 0.5f + size * 0.5f, 0.0f));
}

inline Sprite* Resolve(TileMapEditor* editor, const glm::ivec2& cell, TileId id) {
    TileSet* set = editor->GetTileSet();
    if (!set) return nullptr;
    const std::string spriteId = set->ResolveSprite(id, editor->GetNeighbors(cell));
    return spriteId.empty() ? nullptr : AssetManager::Get().GetSprite(spriteId);
}

inline bool Visible(const glm::mat4& model, const glm::vec2& size,
                    const glm::vec2& pivot, const glm::mat4& viewProjection) {
    glm::vec2 min(1.0e30f), max(-1.0e30f);
    for (float x : {-0.5f, 0.5f}) for (float y : {-0.5f, 0.5f}) {
        const glm::vec4 clip = viewProjection * model
            * glm::vec4((glm::vec2(x, y) - pivot) * size, 0.0f, 1.0f);
        if (clip.w == 0.0f) continue;
        const glm::vec2 point = glm::vec2(clip) / clip.w;
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    return max.x >= -1.0f && min.x <= 1.0f && max.y >= -1.0f && min.y <= 1.0f;
}

}
