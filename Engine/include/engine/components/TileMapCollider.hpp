#pragma once

#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/components/Collider.hpp"

class Container;
class Grid;
class TileMap;
class TileMapEditor;
class TileSet;

class TileMapCollider : public Collider {
public:
    struct GeneratedRectangle {
        glm::vec2 center{0.0f};
        glm::vec2 size{0.0f};
        bool operator==(const GeneratedRectangle&) const = default;
    };
    struct GeneratedPolygon {
        std::vector<glm::vec2> points;
        bool operator==(const GeneratedPolygon&) const = default;
    };

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void FixedUpdate() override;
    void Shutdown() override;
    void OnEnabled() override;
    void OnDisabled() override;
    void OnTransformScaleUpdate() override;
    void CreateShape() override;

    void SetCenter(glm::vec2 value) override;
    void SetDensity(float value) override;
    void SetBounciness(float value) override;
    void SetIsSensor(bool value) override;
    void SetFriction(float value) override;
    void SetRollingResistance(float value) override;

    TileMapCollider* Copy() override;
    TileMapCollider* Copy(Container* container) override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileMapCollider"; }

    const std::vector<GeneratedRectangle>& GetGeneratedRectangles() const {
        return generatedRectangles;
    }
    const std::vector<GeneratedPolygon>& GetGeneratedPolygons() const {
        return generatedPolygons;
    }
    std::size_t GetGeneratedShapeCount() const { return shapeIds.size(); }
    bool IsGeometryDirty() const { return geometryDirty; }

private:
    struct SubscriptionIds {
        std::vector<int> editor;
        std::vector<int> tileMap;
        std::vector<int> tileSet;
        std::vector<int> grid;
        std::vector<int> transform;
        std::vector<int> renderer;
        std::vector<std::pair<std::string, int>> sprites;
    };

    void BindSources();
    void ClearSourceSubscriptions();
    void MarkGeometryDirty();
    void DestroyShapes();
    void ApplyMass();
    std::vector<GeneratedRectangle> BuildRectangles(
        TileMap* map, TileSet* set, Grid* grid,
        std::vector<GeneratedPolygon>& polygons) const;

    TileMapEditor* tileMapEditor = nullptr;
    std::vector<b2ShapeId> shapeIds;
    std::vector<GeneratedRectangle> generatedRectangles;
    std::vector<GeneratedPolygon> generatedPolygons;
    SubscriptionIds subscriptionIds;
    std::string editorSourceId;
    std::string tileMapSourceId;
    std::string tileSetSourceId;
    std::string gridSourceId;
    std::string transformSourceId;
    std::string rendererSourceId;
    bool tileMapAcquired = false;
    bool geometryDirty = true;
};
