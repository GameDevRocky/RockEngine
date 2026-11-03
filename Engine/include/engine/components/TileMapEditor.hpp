#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/components/Component.hpp"
#include "engine/tilemap/TileMap.hpp"


class Container;
class Grid;
class TileSet;

class TileMapEditor : public Component {
public:
    static inline const Event TILEMAP_CHANGED_EVENT = TileMapEditor::CreateEvent();
    static inline const Event TILESET_CHANGED_EVENT = TileMapEditor::CreateEvent();
    static inline const Event GRID_CHANGED_EVENT = TileMapEditor::CreateEvent();

    void Init() override;
    void PostInit() override;
    void Shutdown() override;
    void OnEnabled() override;
    void OnDisabled() override;

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    TileMapEditor* Copy() override;
    TileMapEditor* Copy(Container* container) override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileMapEditor"; }

    Grid* GetGrid() const;
    const std::string& GetGridId() const { return gridId; }
    TileMap* GetTileMap() const;
    TileSet* GetTileSet() const;
    const std::string& GetTileMapId() const { return tileMapId; }
    const std::string& GetTileSetId() const { return tileSetId; }

    void SetTileMap(TileMap* map);
    void SetTileSet(TileSet* set);

    TileId GetTileId(const glm::ivec2& cell) const;
    bool SetTileId(const glm::ivec2& cell, TileId tileId);
    bool ClearTile(const glm::ivec2& cell);
    NeighborMask GetNeighbors(const glm::ivec2& cell) const;

    glm::ivec2 WorldToCell(const glm::vec2& world) const;
    glm::vec2 CellToWorld(const glm::ivec2& cell) const;

private:
    struct GridSubscription {
        std::string sourceId;
        int subscriptionId;
    };

    void RefreshGrid(const std::string& removedGridId = {});
    void SubscribeToGridHierarchy(RuntimeObject* source, Event event);
    void ClearGridSubscriptions();
    void AcquireTileMap();
    void ReleaseTileMap();
    std::string gridId;
    std::vector<GridSubscription> gridSubscriptions;
    std::string tileMapId;
    std::string tileSetId;
    bool tileMapAcquired = false;
};
