#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QWidget>

#include "engine/tilemap/Tile.hpp"
#include "utils/PropertyWidget.hpp"

class TileSet;
class QLineEdit;
class QSpinBox;

struct TileSetTileEntry {
    TileId id = EMPTY_TILE_ID;
    std::string name;
    std::string defaultSpriteId;
    bool selected = false;
    bool operator==(const TileSetTileEntry&) const = default;
};

struct TileSetTileOption {
    TileId id = EMPTY_TILE_ID;
    std::string name;
    std::string defaultSpriteId;
    bool operator==(const TileSetTileOption&) const = default;
};

struct TileSetRuleEntry {
    NeighborPattern mask = [] {
        NeighborPattern pattern{};
        pattern.fill(EMPTY_TILE_ID);
        return pattern;
    }();
    std::string spriteId;
    TileId ownerId = EMPTY_TILE_ID;
    std::vector<TileSetTileOption> options;
    bool newlyAdded = true;
    bool operator==(const TileSetRuleEntry&) const = default;
};

class TileSetInspectorWidget final : public QWidget {
public:
    explicit TileSetInspectorWidget(TileSet* tileSet, QWidget* parent = nullptr);
    void Refresh();

private:
    void RefreshTiles();
    void SelectTile(TileId id);
    void RefreshRules();
    void QueueRefresh();
    void HandleTileListChanged(const std::vector<TileSetTileEntry>& entries);
    void HandleRuleListChanged(const std::vector<TileSetRuleEntry>& entries);
    void CommitTileId();
    void CommitTileName();

    TileSet* m_tileSet = nullptr;
    TileId m_selectedTileId = EMPTY_TILE_ID;
    std::unique_ptr<PropertyWidget<std::vector<TileSetTileEntry>>> m_tiles;
    QWidget* m_tileFields = nullptr;
    QSpinBox* m_idField = nullptr;
    QLineEdit* m_nameField = nullptr;
    std::unique_ptr<PropertyWidget<std::string>> m_defaultSprite;
    std::unique_ptr<PropertyWidget<std::vector<TileSetRuleEntry>>> m_rules;
    bool m_refreshQueued = false;
};
