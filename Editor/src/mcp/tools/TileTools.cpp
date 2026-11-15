#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/UserClarification.hpp"

#include "engine/components/TileMapEditor.hpp"
#include "engine/core/Command.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"

#include <QJsonArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace mcp {
namespace {

bool Integer(const QJsonValue& value, int low, int high, int& out) {
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < low || number > high)
        return false;
    out = static_cast<int>(number);
    return true;
}

McpResult ResolveTileSet(const QJsonObject& params, TileSet** out) {
    const QString id = params.value("assetId").toString();
    if (id.isEmpty()) return McpResult::Error(InvalidParams, "missing assetId");
    *out = AssetManager::Get().GetTileSet(id.toStdString());
    return *out ? McpResult::Ok() : McpResult::Error(ObjectNotFound, "no TileSet with id " + id);
}

McpResult ResolveTileMap(const QJsonObject& params, TileMap** out) {
    const QString id = params.value("assetId").toString();
    if (id.isEmpty()) return McpResult::Error(InvalidParams, "missing assetId");
    *out = AssetManager::Get().GetTileMap(id.toStdString());
    return *out ? McpResult::Ok() : McpResult::Error(ObjectNotFound, "no TileMap with id " + id);
}

McpResult ResolveTileId(const QJsonObject& params, TileId& out) {
    int id = 0;
    if (!Integer(params.value("tileId"), 1, std::numeric_limits<TileId>::max(), id))
        return McpResult::Error(InvalidParams, "tileId must be an integer from 1 to 65535");
    out = static_cast<TileId>(id);
    return McpResult::Ok();
}

QJsonValue TileJson(TileSet* set, TileId id) {
    const Tile* tile = set->GetTile(id);
    if (!tile) return {};
    auto collisionName = [](TileCollisionType type) -> QString {
        switch (type) {
            case TileCollisionType::Sprite: return "Sprite";
            case TileCollisionType::Grid: return "Grid";
            case TileCollisionType::Custom: return "Custom";
            default: return "None";
        }
    };
    auto polygonJson = [](const TileCollisionPolygon& polygon) {
        QJsonArray points;
        for (const auto& point : polygon) points.append(QJsonArray{point.x, point.y});
        return points;
    };
    QJsonArray rules;
    for (const AutoTileRule& rule : tile->autoTileRules) {
        QJsonArray neighbors;
        for (const auto& neighbor : rule.neighbors)
            neighbors.append(neighbor ? QJsonValue(static_cast<int>(*neighbor)) : QJsonValue("any"));
        QJsonArray sprites;
        for (const auto& sprite : rule.spriteIds) sprites.append(QString::fromStdString(sprite));
        QString output = "Fixed";
        if (rule.outputType == TileOutputType::Random) output = "Random";
        if (rule.outputType == TileOutputType::Animation) output = "Animation";
        rules.append(QJsonObject{{"neighbors", neighbors}, {"spriteId", QString::fromStdString(rule.spriteId)},
                                 {"collisionType", collisionName(rule.collisionType)},
                                 {"customPolygon", polygonJson(rule.customPolygon)},
                                 {"output", output}, {"sprites", sprites},
                                 {"minSpeed", rule.minAnimationSpeed}, {"maxSpeed", rule.maxAnimationSpeed}});
    }
    return QJsonObject{{"id", id}, {"name", QString::fromStdString(tile->name)},
                       {"defaultSpriteId", QString::fromStdString(tile->defaultSpriteId)},
                       {"collisionType", collisionName(tile->collisionType)},
                       {"customPolygon", polygonJson(tile->customPolygon)}, {"rules", rules}};
}

bool ParseCollision(const QJsonValue& value, TileCollisionType& out) {
    const QString name = value.toString("None");
    if (name == "None") out = TileCollisionType::None;
    else if (name == "Sprite") out = TileCollisionType::Sprite;
    else if (name == "Grid") out = TileCollisionType::Grid;
    else if (name == "Custom") out = TileCollisionType::Custom;
    else return false;
    return true;
}

bool ParsePolygon(const QJsonValue& value, TileCollisionPolygon& out) {
    if (value.isUndefined()) return true;
    if (!value.isArray()) return false;
    out.clear();
    for (const QJsonValue& point : value.toArray()) {
        if (!point.isArray() || point.toArray().size() != 2) return false;
        const QJsonArray pair = point.toArray();
        if (!pair[0].isDouble() || !pair[1].isDouble()) return false;
        out.emplace_back(static_cast<float>(pair[0].toDouble()),
                         static_cast<float>(pair[1].toDouble()));
    }
    return IsValidTileCollisionPolygon(out);
}

bool ValidSprite(const std::string& id) {
    return id.empty() || AssetManager::Get().GetSprite(id) != nullptr;
}

QString ParseTile(const QJsonObject& input, Tile& tile) {
    if (!input.value("name").isString()) return "tile.name must be a string";
    tile.name = input.value("name").toString().toStdString();
    if (!input.value("defaultSpriteId").isString()) return "tile.defaultSpriteId must be a string";
    tile.defaultSpriteId = input.value("defaultSpriteId").toString().toStdString();
    if (!ValidSprite(tile.defaultSpriteId)) return "tile.defaultSpriteId is not a loaded Sprite";
    if ((!input.value("collisionType").isUndefined() && !input.value("collisionType").isString()) ||
        !ParseCollision(input.value("collisionType"), tile.collisionType))
        return "tile.collisionType must be None, Sprite, Grid or Custom";
    if (!ParsePolygon(input.value("customPolygon"), tile.customPolygon))
        return "tile.customPolygon must be a convex 3-8 point polygon inside [-0.5, 0.5]";
    if (!input.value("rules").isArray()) return "tile.rules must be an array";
    for (const QJsonValue& value : input.value("rules").toArray()) {
        if (!value.isObject()) return "each rule must be an object";
        const QJsonObject ruleInput = value.toObject();
        AutoTileRule rule;
        const QJsonArray neighbors = ruleInput.value("neighbors").toArray();
        if (neighbors.size() != 8) return "rule.neighbors must contain 8 values";
        for (int i = 0; i < 8; ++i) {
            if (neighbors[i].isString() && neighbors[i].toString() == "any") continue;
            int neighbor = 0;
            if (!Integer(neighbors[i], 0, std::numeric_limits<TileId>::max(), neighbor))
                return "each neighbor must be any or a tile ID from 0 to 65535";
            rule.neighbors[i] = static_cast<TileId>(neighbor);
        }
        if ((!ruleInput.value("collisionType").isUndefined() && !ruleInput.value("collisionType").isString()) ||
            !ParseCollision(ruleInput.value("collisionType"), rule.collisionType))
            return "rule.collisionType must be None, Sprite, Grid or Custom";
        if (!ParsePolygon(ruleInput.value("customPolygon"), rule.customPolygon))
            return "rule.customPolygon must be a convex 3-8 point polygon inside [-0.5, 0.5]";
        if (!ruleInput.value("output").isUndefined() && !ruleInput.value("output").isString())
            return "rule.output must be Fixed, Random or Animation";
        const QString output = ruleInput.value("output").toString("Fixed");
        if (output == "Fixed") rule.outputType = TileOutputType::Fixed;
        else if (output == "Random") rule.outputType = TileOutputType::Random;
        else if (output == "Animation") rule.outputType = TileOutputType::Animation;
        else return "rule.output must be Fixed, Random or Animation";
        if (rule.outputType == TileOutputType::Fixed) {
            if (!ruleInput.value("spriteId").isString()) return "fixed rule.spriteId must be a string";
            rule.spriteId = ruleInput.value("spriteId").toString().toStdString();
            if (!ValidSprite(rule.spriteId)) return "rule.spriteId is not a loaded Sprite";
        } else {
            if (!ruleInput.value("sprites").isArray()) return "rule.sprites must be an array";
            for (const QJsonValue& sprite : ruleInput.value("sprites").toArray()) {
                if (!sprite.isString()) return "rule.sprites must contain Sprite IDs";
                const std::string id = sprite.toString().toStdString();
                if (!ValidSprite(id)) return "rule.sprites contains an unknown Sprite ID";
                rule.spriteIds.push_back(id);
            }
        }
        if (rule.outputType == TileOutputType::Animation) {
            if ((!ruleInput.value("minSpeed").isUndefined() && !ruleInput.value("minSpeed").isDouble()) ||
                (!ruleInput.value("maxSpeed").isUndefined() && !ruleInput.value("maxSpeed").isDouble()))
                return "animation speeds must be numbers";
            const double min = ruleInput.value("minSpeed").toDouble(1.0);
            const double max = ruleInput.value("maxSpeed").toDouble(1.0);
            if (!std::isfinite(min) || !std::isfinite(max) || min < 0.01 || max < min)
                return "animation speeds must be finite, with 0.01 <= minSpeed <= maxSpeed";
            rule.minAnimationSpeed = static_cast<float>(min);
            rule.maxAnimationSpeed = static_cast<float>(max);
        }
        tile.autoTileRules.push_back(std::move(rule));
    }
    for (std::size_t i = 0; i < tile.autoTileRules.size(); ++i)
        for (std::size_t j = i + 1; j < tile.autoTileRules.size(); ++j)
            if (tile.autoTileRules[i].neighbors == tile.autoTileRules[j].neighbors)
                return "tile.rules contains duplicate neighbor patterns";
    return {};
}

struct CellChange { glm::ivec2 cell; TileId before; TileId after; };

class TileCellsCommand final : public Command {
public:
    TileCellsCommand(std::string mapId, std::vector<CellChange> changes)
        : mapId(std::move(mapId)), changes(std::move(changes)) {}
    void Undo(Container*) override { Apply(false); }
    void Redo(Container*) override { Apply(true); }
    std::string GetText() const override { return "Edit TileMap Cells"; }
private:
    void Apply(bool after) {
        TileMap* map = AssetManager::Get().GetTileMap(mapId);
        if (!map) return;
        const bool temporary = !map->GetCells();
        if (temporary && !map->Load()) return;
        for (const CellChange& change : changes)
            map->SetTileId(change.cell, after ? change.after : change.before);
        AssetManager::Get().SaveResource(map);
        if (temporary) map->Unload();
    }
    std::string mapId;
    std::vector<CellChange> changes;
};

} // namespace

void RegisterTileTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("tileset.list_tiles", [](const QJsonObject& params) {
        TileSet* set = nullptr;
        if (McpResult r = ResolveTileSet(params, &set); !r.ok) return r;
        QJsonArray tiles;
        std::vector<TileId> ids;
        for (const auto& [id, tile] : set->GetTiles()) ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (TileId id : ids) {
            const Tile& tile = *set->GetTile(id);
            tiles.append(QJsonObject{{"tileId", id}, {"name", QString::fromStdString(tile.name)},
                                     {"ruleCount", static_cast<int>(tile.autoTileRules.size())}});
        }
        return McpResult::Ok(QJsonObject{{"assetId", params.value("assetId")}, {"tiles", tiles}});
    });
    dispatcher.RegisterTool("tileset.get_tile", [](const QJsonObject& params) {
        TileSet* set = nullptr;
        if (McpResult r = ResolveTileSet(params, &set); !r.ok) return r;
        TileId id;
        if (McpResult r = ResolveTileId(params, id); !r.ok) return r;
        if (!set->GetTile(id)) return McpResult::Error(ObjectNotFound, "tile ID does not exist");
        return McpResult::Ok(TileJson(set, id));
    });
    dispatcher.RegisterTool("tileset.set_tile", [](const QJsonObject& params) {
        if (support::IsRuntimeWorld()) return McpResult::Error(WrongMode, "TileSet authoring requires the editor world");
        TileSet* set = nullptr;
        if (McpResult r = ResolveTileSet(params, &set); !r.ok) return r;
        TileId id;
        if (McpResult r = ResolveTileId(params, id); !r.ok) return r;
        if (!params.value("tile").isObject())
            return McpResult::Error(InvalidParams, "tile must be a complete tile definition object");
        Tile tile;
        const QString error = ParseTile(params.value("tile").toObject(), tile);
        if (!error.isEmpty()) return McpResult::Error(InvalidParams, error);
        if (!set->SetTile(id, std::move(tile))) return McpResult::Error(InvalidParams, "invalid tile definition");
        if (!AssetManager::Get().SaveResource(set)) return McpResult::Error(WrongMode, "tile changed in memory but TileSet could not be saved");
        return McpResult::Ok(QJsonObject{{"assetId", params.value("assetId")}, {"tile", TileJson(set, id)}, {"saved", true}});
    });
    dispatcher.RegisterTool("tileset.remove_tile", [](const QJsonObject& params) {
        if (support::IsRuntimeWorld()) return McpResult::Error(WrongMode, "TileSet authoring requires the editor world");
        TileSet* set = nullptr;
        if (McpResult r = ResolveTileSet(params, &set); !r.ok) return r;
        TileId id;
        if (McpResult r = ResolveTileId(params, id); !r.ok) return r;
        if (!set->GetTile(id)) return McpResult::Error(ObjectNotFound, "tile ID does not exist");
        const QByteArray definition = QJsonDocument(TileJson(set, id).toObject()).toJson(QJsonDocument::Compact);
        const QString scope = "tileset.remove:" + params.value("assetId").toString() + ":" +
            QString::number(id) + ":" + QString::fromLatin1(QCryptographicHash::hash(definition, QCryptographicHash::Sha256).toHex());
        const QString clarificationId = params.value("clarificationId").toString();
        if (clarificationId.isEmpty()) {
            int affectedCells = 0;
            QJsonArray affectedMaps;
            for (const auto& [mapId, map] : AssetManager::Get().GetAllTileMaps()) {
                if (!map) continue;
                const bool temporary = !map->GetCells();
                if (temporary && !map->Load()) continue;
                int count = 0;
                for (const auto& [cell, tileId] : *map->GetCells())
                    if (tileId == id) ++count;
                if (temporary) map->Unload();
                if (count) {
                    affectedCells += count;
                    affectedMaps.append(QJsonObject{{"assetId", QString::fromStdString(mapId)}, {"cellCount", count}});
                }
            }
            ClarificationRequest request;
            request.title = "Remove TileSet tile";
            request.question = QString("Remove tile %1 from %2?").arg(id).arg(QString::fromStdString(set->GetName()));
            request.context = QString("%1 painted cells across %2 TileMaps use this numeric ID and may be affected. Removing the definition leaves those cells in place, but their sprites and collisions no longer resolve through this TileSet.")
                .arg(affectedCells).arg(affectedMaps.size());
            request.options = {{"remove", "Remove tile", "Delete this TileSet definition"},
                               {"cancel", "Cancel", "Keep the tile"}};
            request.scope = scope;
            const QString requestId = UserClarificationService::Get()->Create(std::move(request));
            return McpResult::Ok(QJsonObject{{"status", "clarification_required"}, {"requestId", requestId},
                                             {"affectedCells", affectedCells}, {"affectedMaps", affectedMaps}});
        }
        QJsonObject clarification;
        QString authorizationError;
        if (!UserClarificationService::Get()->Consume(clarificationId, scope, "remove", &clarification, &authorizationError))
            return McpResult::Error(InvalidParams, authorizationError);
        if (!set->RemoveTile(id)) return McpResult::Error(ObjectNotFound, "tile ID does not exist");
        if (!AssetManager::Get().SaveResource(set)) return McpResult::Error(WrongMode, "tile removed in memory but TileSet could not be saved");
        return McpResult::Ok(QJsonObject{{"assetId", params.value("assetId")}, {"tileId", id},
                                         {"clarification", clarification}, {"saved", true}});
    });
    dispatcher.RegisterTool("tilemap.get_cells", [](const QJsonObject& params) {
        TileMap* map = nullptr;
        if (McpResult r = ResolveTileMap(params, &map); !r.ok) return r;
        const bool temporary = !map->GetCells();
        if (temporary && !map->Load()) return McpResult::Error(WrongMode, "could not load TileMap cells");
        int minX = std::numeric_limits<int>::min(), minY = minX;
        int maxX = std::numeric_limits<int>::max(), maxY = maxX;
        bool valid = true;
        if (params.contains("minX")) valid &= Integer(params.value("minX"), minX, maxX, minX);
        if (params.contains("minY")) valid &= Integer(params.value("minY"), std::numeric_limits<int>::min(), maxY, minY);
        if (params.contains("maxX")) valid &= Integer(params.value("maxX"), minX, maxX, maxX);
        if (params.contains("maxY")) valid &= Integer(params.value("maxY"), minY, std::numeric_limits<int>::max(), maxY);
        int limit = 200;
        if (params.contains("limit")) valid &= Integer(params.value("limit"), 1, 1000, limit);
        if (!valid || minX > maxX || minY > maxY) {
            if (temporary) map->Unload();
            return McpResult::Error(InvalidParams, "invalid cell bounds or limit (1-1000)");
        }
        std::vector<std::pair<glm::ivec2, TileId>> ordered;
        for (const auto& [cell, id] : *map->GetCells())
            if (cell.x >= minX && cell.x <= maxX && cell.y >= minY && cell.y <= maxY)
                ordered.emplace_back(cell, id);
        std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
            return a.first.y == b.first.y ? a.first.x < b.first.x : a.first.y < b.first.y;
        });
        QJsonArray cells;
        for (int i = 0; i < std::min(limit, static_cast<int>(ordered.size())); ++i)
            cells.append(QJsonObject{{"x", ordered[i].first.x}, {"y", ordered[i].first.y}, {"tileId", ordered[i].second}});
        if (temporary) map->Unload();
        return McpResult::Ok(QJsonObject{{"assetId", params.value("assetId")}, {"cells", cells},
                                         {"total", static_cast<int>(ordered.size())},
                                         {"truncated", static_cast<int>(ordered.size()) > limit}});
    });
    dispatcher.RegisterTool("tilemap.set_cells", [](const QJsonObject& params) {
        Container* container = support::ActiveContainer();
        if (!container || container->GetMode() != Container::Mode::Editor)
            return McpResult::Error(WrongMode, "tile cell authoring requires the editor world");
        const QString componentId = params.value("componentId").toString();
        auto* registry = container->FindSystem<Registry>();
        auto* editor = registry ? registry->Find<TileMapEditor>(componentId.toStdString()) : nullptr;
        if (!editor) return McpResult::Error(ObjectNotFound, "no TileMapEditor with id " + componentId);
        TileMap* map = editor->GetTileMap();
        if (!map || !map->GetCells() || !editor->GetGrid())
            return McpResult::Error(WrongMode, "TileMapEditor needs a loaded TileMap and ancestor Grid");
        const QJsonArray requested = params.value("cells").toArray();
        if (requested.isEmpty() || requested.size() > 1000)
            return McpResult::Error(InvalidParams, "cells must contain 1-1000 entries");
        std::map<std::pair<int, int>, TileId> updates;
        for (const QJsonValue& value : requested) {
            if (!value.isObject()) return McpResult::Error(InvalidParams, "each cell must be an object");
            const QJsonObject entry = value.toObject();
            int x, y, id;
            if (!Integer(entry.value("x"), std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), x) ||
                !Integer(entry.value("y"), std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), y) ||
                !Integer(entry.value("tileId"), 0, std::numeric_limits<TileId>::max(), id))
                return McpResult::Error(InvalidParams, "each cell needs integer x, y and tileId (0-65535)");
            if (id && (!editor->GetTileSet() || !editor->GetTileSet()->GetTile(static_cast<TileId>(id))))
                return McpResult::Error(InvalidParams, "tileId is absent from the assigned TileSet");
            updates[{x, y}] = static_cast<TileId>(id);
        }
        std::vector<CellChange> changes;
        for (const auto& [xy, id] : updates) {
            const glm::ivec2 cell{xy.first, xy.second};
            const TileId before = map->GetTileId(cell);
            if (before == id) continue;
            changes.push_back({cell, before, id});
            map->SetTileId(cell, id);
        }
        if (!changes.empty()) {
            if (!AssetManager::Get().SaveResource(map))
                return McpResult::Error(WrongMode, "cells changed in memory but TileMap could not be saved");
            if (auto* undo = container->FindSystem<UndoSystem>())
                undo->Push(std::make_unique<TileCellsCommand>(map->GetID(), changes));
        }
        return McpResult::Ok(QJsonObject{{"componentId", componentId}, {"assetId", QString::fromStdString(map->GetID())},
                                         {"changed", static_cast<int>(changes.size())}, {"saved", true}});
    });
}

} // namespace mcp
