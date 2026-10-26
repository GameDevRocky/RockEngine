#include "engine/components/TileMapEditor.hpp"

#include <array>

#include "engine/components/Grid.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/tilemap/TileSet.hpp"

void TileMapEditor::Init()
{
    if (state >= State::Initialized) return;

    Component::Init();
}

void TileMapEditor::PostInit()
{
    Component::PostInit();
    if (enabled) AcquireTileMap();
}

void TileMapEditor::Shutdown()
{
    ReleaseTileMap();
    gridId.clear();
    Component::Shutdown();
}

void TileMapEditor::OnEnabled()
{
    if (state < State::PostInitialized) return;
    AcquireTileMap();
   
}

void TileMapEditor::OnDisabled()
{
    ReleaseTileMap();
}

YAML::Node TileMapEditor::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["tileMapId"] = tileMapId;
    node["tileSetId"] = tileSetId;
    return node;
}

void TileMapEditor::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    tileMapId = node["tileMapId"].as<std::string>("");
    tileSetId = node["tileSetId"].as<std::string>("");
}

TileMapEditor* TileMapEditor::Copy()
{
    auto* copy = new TileMapEditor();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->tileMapId = tileMapId;
    copy->tileSetId = tileSetId;
    return copy;
}

TileMapEditor* TileMapEditor::Copy(Container* targetContainer)
{
    TileMapEditor* copy = Copy();
    copy->Attach(targetContainer);
    return copy;
}

void TileMapEditor::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

Grid* TileMapEditor::GetGrid() const
{
    return registry->Find<Grid>(gridId);
}

TileMap* TileMapEditor::GetTileMap() const
{
    return AssetManager::Get().GetTileMap(tileMapId);
}

TileSet* TileMapEditor::GetTileSet() const
{
    return AssetManager::Get().GetTileSet(tileSetId);
}

void TileMapEditor::SetTileMap(TileMap* map)
{
    if (!map) return;
    const std::string id = map->GetID();
    if (tileMapId == id) return;
    ReleaseTileMap();
    tileMapId = id;
    if (enabled) AcquireTileMap();
    Notify(TILEMAP_CHANGED_EVENT, tileMapId);
}

void TileMapEditor::SetTileSet(TileSet* set)
{
    const std::string id = set ? set->GetID() : std::string{};
    if (tileSetId == id) return;
    tileSetId = id;
    Notify(TILESET_CHANGED_EVENT, tileSetId);
}

TileTypeId TileMapEditor::GetTileType(const glm::ivec2& cell) const
{
    TileMap* map = GetTileMap();
    if (map == nullptr) return EMPTY_TILE_TYPE;
    return map->GetTileType(cell);
}

bool TileMapEditor::SetTileType(const glm::ivec2& cell, TileTypeId type)
{
    TileMap* map = GetTileMap();
    if (map == nullptr) return false;
    return map->SetTileType(cell, type);
}

bool TileMapEditor::ClearTile(const glm::ivec2& cell)
{
    TileMap* map = GetTileMap();
    if (map == nullptr) return false;
    return map->ClearTile(cell);
}

NeighborMask TileMapEditor::GetNeighbors(const glm::ivec2& cell) const
{
    static constexpr std::array<glm::ivec2, 8> offsets = {
        glm::ivec2{0, 1}, glm::ivec2{1, 1}, glm::ivec2{1, 0}, glm::ivec2{1, -1},
        glm::ivec2{0, -1}, glm::ivec2{-1, -1}, glm::ivec2{-1, 0}, glm::ivec2{-1, 1}
    };

    NeighborMask neighbors{};
    for (std::size_t i = 0; i < offsets.size(); ++i)
        neighbors[i] = GetTileType(cell + offsets[i]);
    return neighbors;
}

glm::ivec2 TileMapEditor::WorldToCell(const glm::vec2& world) const
{
    return GetGrid()->WorldToCell(world);
}

glm::vec2 TileMapEditor::CellToWorld(const glm::ivec2& cell) const
{
    return GetGrid()->CellToWorld(cell);
}

void TileMapEditor::AcquireTileMap()
{
    if (tileMapAcquired) return;
    TileMap* map = GetTileMap();
    if (map == nullptr) return;
    tileMapAcquired = map->Load();
}

void TileMapEditor::ReleaseTileMap()
{
    if (!tileMapAcquired) return;
    if (TileMap* map = GetTileMap()) map->Unload();
    tileMapAcquired = false;
}

