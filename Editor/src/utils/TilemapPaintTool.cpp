#include "utils/SceneEditController.hpp"
#include "utils/SceneEditTools.hpp"
#include "utils/TilePaintState.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/Command.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/rendering/views/EditorRenderView.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include "imgui.h"

namespace {
class TileStrokeCommand final : public Command {
public:
    using Changes = std::map<std::pair<int, int>, std::pair<TileId, TileId>>;
    TileStrokeCommand(std::string mapId, Changes changes)
        : mapId(std::move(mapId)), changes(std::move(changes)) {}
    void Undo(Container*) override { Apply(false); }
    void Redo(Container*) override { Apply(true); }
    std::string GetText() const override { return "Paint Tiles"; }
private:
    void Apply(bool after) {
        TileMap* map = AssetManager::Get().GetTileMap(mapId);
        if (!map) return;
        const bool temporaryLoad = !map->GetCells();
        if (temporaryLoad && !map->Load()) return;
        for (const auto& [cell, values] : changes) {
            const TileId id = after ? values.second : values.first;
            if (id == EMPTY_TILE_ID) map->ClearTile({cell.first, cell.second});
            else map->SetTileId({cell.first, cell.second}, id);
        }
        AssetManager::Get().SaveResource(map);
        if (temporaryLoad) map->Unload();
    }
    std::string mapId;
    Changes changes;
};

class TilemapPaintTool final : public SceneEditTool {
public:
    bool PointerPress(const SceneEditContext& context, const SceneEditPointerEvent& event) override {
        if (event.button != Qt::LeftButton ||
            GizmosManager::Get()->GetEditMode() != GizmosManager::EditMode::Tilemap)
            return false;
        TileMapEditor* editor = ActiveEditor(context);
        if (!editor) return false;
        strokeMapId = editor->GetTileMap()->GetID();
        strokeContainer = context.container;
        strokeTileId = TilePaintState::Get().selectedTileId;
        strokeBrushSize = TilePaintState::Get().brushSize;
        lastCell = CellAt(context, editor, event.position);
        changes.clear();
        active = true;
        PaintTo(context, event.position);
        return true;
    }
    bool PointerMove(const SceneEditContext& context, const SceneEditPointerEvent& event) override {
        hoverPoint = event.position;
        hoverValid = event.inside;
        if (active && (event.buttons & Qt::LeftButton)) {
            PaintTo(context, event.position);
            return true;
        }
        if (active) {
            Finish(context);
            return true;
        }
        return false;
    }
    void PointerLeave(const SceneEditContext& context) override {
        hoverValid = false;
        if (context.requestRepaint) context.requestRepaint();
    }
    bool PointerRelease(const SceneEditContext& context, const SceneEditPointerEvent& event) override {
        if (!active || event.button != Qt::LeftButton) return false;
        PaintTo(context, event.position);
        Finish(context);
        return true;
    }
    void Finish(const SceneEditContext& context) override {
        if (!active) return;
        active = false;
        if (!changes.empty()) {
            if (TileMap* map = AssetManager::Get().GetTileMap(strokeMapId))
                AssetManager::Get().SaveResource(map);
            if (strokeContainer && strokeContainer->GetMode() == Container::Mode::Editor)
                if (auto* undo = strokeContainer->FindSystem<UndoSystem>())
                    undo->Push(std::make_unique<TileStrokeCommand>(strokeMapId, changes));
            changes.clear();
        }
        strokeMapId.clear();
        strokeContainer = nullptr;
    }
    void Draw(const SceneEditContext& context) override {
        if (!hoverValid || !context.view ||
            GizmosManager::Get()->GetEditMode() != GizmosManager::EditMode::Tilemap)
            return;
        TileMapEditor* editor = ActiveEditor(context);
        if (!editor) return;
        const glm::ivec2 cell = CellAt(context, editor, hoverPoint);
        Grid* grid = editor->GetGrid();
        const int brushSize = std::max(1, TilePaintState::Get().brushSize);
        const int start = -(brushSize - 1) / 2;
        const glm::ivec2 first = cell + glm::ivec2(start);
        const glm::ivec2 last = first + glm::ivec2(brushSize - 1);
        const glm::vec2 stride = grid->GetSize() + grid->GetGap();
        const glm::vec2 origin = glm::vec2(first) * stride;
        const glm::mat4 world = editor->GetTransform()->GetWorldMatrix();
        auto* camera = context.view->GetCamera();
        const glm::mat4 vp = camera->GetProjectionMatrix() * camera->GetViewMatrix();
        const glm::vec2 size = glm::vec2(last - first) * stride + grid->GetSize();
        ImVec2 points[4];
        for (int i = 0; i < 4; ++i) {
            const glm::vec2 corner = origin + glm::vec2((i == 1 || i == 2) ? size.x : 0.0f,
                                                        i >= 2 ? size.y : 0.0f);
            const glm::vec4 clip = vp * world * glm::vec4(corner, 0.0f, 1.0f);
            if (clip.w == 0.0f) return;
            points[i] = ImVec2((clip.x / clip.w * 0.5f + 0.5f) * context.width,
                               (0.5f - clip.y / clip.w * 0.5f) * context.height);
        }
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const ImU32 color = TilePaintState::Get().selectedTileId == EMPTY_TILE_ID
            ? IM_COL32(255, 105, 90, 220) : IM_COL32(60, 225, 150, 220);
        for (int i = 0; i < 4; ++i) draw->AddLine(points[i], points[(i + 1) % 4], color, 2.0f);
    }
private:
    TileMapEditor* ActiveEditor(const SceneEditContext& context) const {
        if (!context.container || context.container->GetMode() != Container::Mode::Editor) return nullptr;
        auto* selection = context.container->FindSystem<SelectionManager>();
        auto* registry = context.container->FindSystem<Registry>();
        if (!selection || !registry) return nullptr;
        GameObject* object = registry->Find<GameObject>(selection->GetPrimaryId());
        TileMapEditor* editor = object ? object->GetComponent<TileMapEditor>() : nullptr;
        if (!editor || editor->GetID() != TilePaintState::Get().componentId || !editor->GetEnabled()
            || !editor->GetGrid() || !editor->GetTileMap()
            || !editor->GetTileMap()->GetCells()) return nullptr;
        if (!active && TilePaintState::Get().selectedTileId != EMPTY_TILE_ID &&
            (!editor->GetTileSet() || !editor->GetTileSet()->GetTile(TilePaintState::Get().selectedTileId)))
            return nullptr;
        return editor;
    }
    glm::ivec2 CellAt(const SceneEditContext& context, TileMapEditor* editor,
                      const QPoint& point) const {
        return editor->WorldToCell(context.ScreenToWorld(point));
    }
    void PaintTo(const SceneEditContext& context, const QPoint& point) {
        TileMapEditor* editor = ActiveEditor(context);
        if (!active || !editor || editor->GetTileMap()->GetID() != strokeMapId) return;
        const glm::ivec2 end = CellAt(context, editor, point);
        int x = lastCell.x, y = lastCell.y;
        const int dx = std::abs(end.x - x), dy = std::abs(end.y - y);
        const int sx = x < end.x ? 1 : -1, sy = y < end.y ? 1 : -1;
        int error = dx - dy;
        for (;;) {
            const int start = -(strokeBrushSize - 1) / 2;
            for (int row = 0; row < strokeBrushSize; ++row) {
                for (int column = 0; column < strokeBrushSize; ++column) {
                    const glm::ivec2 cell{x + start + column, y + start + row};
                    const TileId before = editor->GetTileId(cell);
                    if (before == strokeTileId) continue;
                    auto [it, inserted] = changes.try_emplace(
                        std::pair{cell.x, cell.y}, before, strokeTileId);
                    if (!inserted) it->second.second = strokeTileId;
                    if (strokeTileId == EMPTY_TILE_ID) editor->ClearTile(cell);
                    else editor->SetTileId(cell, strokeTileId);
                }
            }
            if (x == end.x && y == end.y) break;
            const int twiceError = 2 * error;
            if (twiceError > -dy) { error -= dy; x += sx; }
            if (twiceError < dx) { error += dx; y += sy; }
        }
        lastCell = end;
        if (context.requestRepaint) context.requestRepaint();
    }
    bool active = false;
    bool hoverValid = false;
    QPoint hoverPoint;
    glm::ivec2 lastCell{0};
    TileId strokeTileId = EMPTY_TILE_ID;
    int strokeBrushSize = 1;
    std::string strokeMapId;
    Container* strokeContainer = nullptr;
    TileStrokeCommand::Changes changes;
};

} // namespace

std::unique_ptr<SceneEditTool> CreateTilemapPaintTool() {
    return std::make_unique<TilemapPaintTool>();
}
