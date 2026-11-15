#include "engine/components/TileMapCollider.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "Engine.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"
#include "engine/tilemap/TileRender.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/Hash.hpp"
#include "engine/utils/IVisitor.hpp"

namespace {
constexpr float kGapEpsilon = 1.0e-5f;

TileMapCollider* FindCollider(const std::string& componentId, const std::string& containerId)
{
    for (Container* world : {Engine::Get()->GetEditorContainer(), Engine::Get()->GetRuntimeContainer()}) {
        if (!world || world->GetID() != containerId) continue;
        Registry* registry = world->FindSystem<Registry>();
        return registry ? registry->Find<TileMapCollider>(componentId) : nullptr;
    }
    return nullptr;
}
}

void TileMapCollider::Init()
{
    if (state >= State::Initialized) return;
    tileMapEditor = GetComponent<TileMapEditor>();
    if (!tileMapEditor) tileMapEditor = RequireComponent<TileMapEditor>();
    const bool hadRigidBody = GetComponent<RigidBody>() != nullptr;
    Collider::Init();
    if (!hadRigidBody && rigidBody) rigidBody->SetBodyType(b2_staticBody);
}

void TileMapCollider::PostInit()
{
    if (state >= State::PostInitialized) return;
    Component::PostInit();
    if (Transform* transform = GetTransform()) cachedScale = transform->GetWorldScale();
    tileMapEditor = GetComponent<TileMapEditor>();
    if (enabled) BindSources();
    geometryDirty = true;
    state = State::PostInitialized;
}

void TileMapCollider::Awake()
{
    if (state >= State::Awakened) return;
    if (enabled) CreateShape();
    state = State::Awakened;
}

void TileMapCollider::FixedUpdate()
{
    if (geometryDirty) CreateShape();
}

void TileMapCollider::MarkGeometryDirty()
{
    geometryDirty = true;
}

void TileMapCollider::BindSources()
{
    ClearSourceSubscriptions();
    if (!container || IsMarkedForDestroy()) return;
    tileMapEditor = GetComponent<TileMapEditor>();
    if (!tileMapEditor) return;

    const std::string componentId = GetID();
    const std::string containerId = container->GetID();
    editorSourceId = tileMapEditor->GetID();
    auto rebind = [componentId, containerId]() {
        TileMapCollider* collider = FindCollider(componentId, containerId);
        if (!collider || collider->IsMarkedForDestroy()) return false;
        collider->BindSources();
        collider->MarkGeometryDirty();
        return true;
    };
    subscriptionIds.editor.push_back(tileMapEditor->Subscribe(rebind, TileMapEditor::TILEMAP_CHANGED_EVENT));
    subscriptionIds.editor.push_back(tileMapEditor->Subscribe(rebind, TileMapEditor::TILESET_CHANGED_EVENT));
    subscriptionIds.editor.push_back(tileMapEditor->Subscribe(rebind, TileMapEditor::GRID_CHANGED_EVENT));

    auto dirty = [componentId, containerId]() {
        TileMapCollider* collider = FindCollider(componentId, containerId);
        if (!collider || collider->IsMarkedForDestroy()) return false;
        collider->MarkGeometryDirty();
        return true;
    };

    if (TileMap* map = tileMapEditor->GetTileMap()) {
        tileMapSourceId = map->GetID();
        tileMapAcquired = map->Load();
        subscriptionIds.tileMap.push_back(map->Subscribe(dirty, TileMap::CELL_CHANGED_EVENT));
        subscriptionIds.tileMap.push_back(map->Subscribe(dirty, TileMap::CLEARED_EVENT));
    }
    if (TileSet* set = tileMapEditor->GetTileSet()) {
        tileSetSourceId = set->GetID();
        subscriptionIds.tileSet.push_back(set->Subscribe(rebind, TileSet::CHANGED_EVENT));
        std::unordered_set<std::string> spriteIds;
        for (const auto& [_, tile] : set->GetTiles()) {
            if (!tile.defaultSpriteId.empty()) spriteIds.insert(tile.defaultSpriteId);
            for (const AutoTileRule& rule : tile.autoTileRules) {
                if (!rule.spriteId.empty()) spriteIds.insert(rule.spriteId);
                for (const std::string& spriteId : rule.spriteIds)
                    if (!spriteId.empty()) spriteIds.insert(spriteId);
            }
        }
        for (const std::string& spriteId : spriteIds)
            if (Sprite* sprite = AssetManager::Get().GetSprite(spriteId))
                subscriptionIds.sprites.push_back({spriteId, sprite->Subscribe(dirty)});
    }
    if (Grid* grid = tileMapEditor->GetGrid()) {
        gridSourceId = grid->GetID();
        subscriptionIds.grid.push_back(grid->Subscribe(dirty, Grid::SIZE_CHANGED_EVENT));
        subscriptionIds.grid.push_back(grid->Subscribe(dirty, Grid::GAP_CHANGED_EVENT));
    }
    if (Transform* transform = GetTransform()) {
        transformSourceId = transform->GetID();
        subscriptionIds.transform.push_back(transform->Subscribe(
            [componentId, containerId]() {
                TileMapCollider* collider = FindCollider(componentId, containerId);
                if (!collider || collider->IsMarkedForDestroy()) return false;
                collider->OnTransformScaleUpdate();
                return true;
            }, Transform::SCALE_CHANGED_EVENT));
    }
    if (TileMapRenderer* renderer = GetComponent<TileMapRenderer>()) {
        rendererSourceId = renderer->GetID();
        subscriptionIds.renderer.push_back(renderer->Subscribe(
            dirty, TileMapRenderer::RENDER_CHANGED_EVENT));
    }
}

void TileMapCollider::ClearSourceSubscriptions()
{
    if (registry && !editorSourceId.empty()) {
        if (TileMapEditor* source = registry->Find<TileMapEditor>(editorSourceId))
            for (int id : subscriptionIds.editor) source->Unsubscribe(id);
    }
    if (!tileMapSourceId.empty()) {
        if (TileMap* source = AssetManager::Get().GetTileMap(tileMapSourceId)) {
            for (int id : subscriptionIds.tileMap) source->Unsubscribe(id);
            if (tileMapAcquired) source->Unload();
        }
    }
    if (!tileSetSourceId.empty()) {
        if (TileSet* source = AssetManager::Get().GetTileSet(tileSetSourceId))
            for (int id : subscriptionIds.tileSet) source->Unsubscribe(id);
    }
    if (registry && !gridSourceId.empty()) {
        if (Grid* source = registry->Find<Grid>(gridSourceId))
            for (int id : subscriptionIds.grid) source->Unsubscribe(id);
    }
    if (registry && !transformSourceId.empty()) {
        if (Transform* source = registry->Find<Transform>(transformSourceId))
            for (int id : subscriptionIds.transform) source->Unsubscribe(id);
    }
    if (registry && !rendererSourceId.empty()) {
        if (TileMapRenderer* source = registry->Find<TileMapRenderer>(rendererSourceId))
            for (int id : subscriptionIds.renderer) source->Unsubscribe(id);
    }
    for (const auto& [spriteId, subscriptionId] : subscriptionIds.sprites)
        if (Sprite* source = AssetManager::Get().GetSprite(spriteId)) source->Unsubscribe(subscriptionId);
    subscriptionIds = {};
    editorSourceId.clear();
    tileMapSourceId.clear();
    tileSetSourceId.clear();
    gridSourceId.clear();
    transformSourceId.clear();
    rendererSourceId.clear();
    tileMapAcquired = false;
}

std::vector<TileMapCollider::GeneratedRectangle> TileMapCollider::BuildRectangles(
    TileMap* map, TileSet* set, Grid* grid,
    std::vector<GeneratedPolygon>& polygons) const
{
    std::vector<GeneratedRectangle> rectangles;
    if (!map || !set || !grid) return rectangles;
    const TileMap::CellData* cells = map->GetCells();
    if (!cells) return rectangles;
    const glm::vec2 cellSize = grid->GetSize();
    const glm::vec2 gap = grid->GetGap();
    if (cellSize.x <= kGapEpsilon || cellSize.y <= kGapEpsilon) return rectangles;

    std::unordered_set<glm::ivec2, EngineUtils::IVec2Hash> solid;
    std::vector<std::pair<glm::ivec2, GeneratedRectangle>> spriteRectangles;
    solid.reserve(cells->size());
    TileMapRenderer* renderer = tileMapEditor ? tileMapEditor->GetComponent<TileMapRenderer>() : nullptr;
    for (const auto& [cell, tileId] : *cells) {
        const ResolvedTile resolved = set->ResolveTile(
            tileId, tileMapEditor->GetNeighbors(cell), cell, 0.0f);
        if (resolved.collisionType == TileCollisionType::Grid) {
            solid.insert(cell);
        } else if (resolved.collisionType == TileCollisionType::Sprite && renderer) {
            Sprite* sprite = resolved.spriteId.empty()
                ? nullptr : AssetManager::Get().GetSprite(resolved.spriteId);
            if (!sprite) continue;
            const glm::vec2 size = TileRender::Size(grid, renderer, sprite);
            if (size.x <= kGapEpsilon || size.y <= kGapEpsilon) continue;
            const glm::vec2 origin = glm::vec2(cell) * (cellSize + gap);
            const glm::vec2 cellCenter = origin + cellSize * 0.5f;
            spriteRectangles.push_back({cell, {cellCenter - sprite->GetPivot() * size, size}});
        } else if (resolved.collisionType == TileCollisionType::Custom &&
                   resolved.customPolygon && IsValidTileCollisionPolygon(*resolved.customPolygon)) {
            Sprite* sprite = resolved.spriteId.empty()
                ? nullptr : AssetManager::Get().GetSprite(resolved.spriteId);
            const bool hasRenderedSprite = sprite && renderer;
            const glm::vec2 size = hasRenderedSprite
                ? TileRender::Size(grid, renderer, sprite) : cellSize;
            if (size.x <= kGapEpsilon || size.y <= kGapEpsilon) continue;
            const glm::vec2 pivot = hasRenderedSprite ? sprite->GetPivot() : glm::vec2(0.0f);
            const glm::vec2 cellCenter = glm::vec2(cell) * (cellSize + gap) + cellSize * 0.5f;
            const glm::vec2 spriteCenter = cellCenter - pivot * size;
            GeneratedPolygon polygon;
            polygon.points.reserve(resolved.customPolygon->size());
            for (const glm::vec2& point : *resolved.customPolygon)
                polygon.points.push_back(spriteCenter + point * size);
            polygons.push_back(std::move(polygon));
        }
    }

    std::vector<glm::ivec2> ordered(solid.begin(), solid.end());
    std::sort(ordered.begin(), ordered.end(), [](const glm::ivec2& left, const glm::ivec2& right) {
        return left.y != right.y ? left.y < right.y : left.x < right.x;
    });
    std::unordered_set<glm::ivec2, EngineUtils::IVec2Hash> consumed;
    consumed.reserve(solid.size());
    const bool mergeX = gap.x == 0.0f;
    const bool mergeY = gap.y == 0.0f;
    const glm::vec2 stride = cellSize + gap;

    for (const glm::ivec2 start : ordered) {
        if (consumed.contains(start)) continue;
        int width = 1;
        if (mergeX) {
            while (solid.contains({start.x + width, start.y})
                   && !consumed.contains({start.x + width, start.y})) ++width;
        }
        int height = 1;
        if (mergeY) {
            for (;;) {
                bool completeRow = true;
                for (int x = 0; x < width; ++x) {
                    const glm::ivec2 candidate{start.x + x, start.y + height};
                    if (!solid.contains(candidate) || consumed.contains(candidate)) {
                        completeRow = false;
                        break;
                    }
                }
                if (!completeRow) break;
                ++height;
            }
        }
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                consumed.insert({start.x + x, start.y + y});

        const glm::vec2 rectangleSize{
            cellSize.x + static_cast<float>(width - 1) * stride.x,
            cellSize.y + static_cast<float>(height - 1) * stride.y
        };
        const glm::vec2 origin = glm::vec2(start) * stride;
        rectangles.push_back({origin + rectangleSize * 0.5f, rectangleSize});
    }
    std::sort(spriteRectangles.begin(), spriteRectangles.end(), [](const auto& left, const auto& right) {
        return left.first.y != right.first.y ? left.first.y < right.first.y : left.first.x < right.first.x;
    });
    for (const auto& [_, rectangle] : spriteRectangles) rectangles.push_back(rectangle);
    return rectangles;
}

void TileMapCollider::CreateShape()
{
    geometryDirty = false;
    DestroyShapes();
    if (!rigidBody || !b2Body_IsValid(rigidBody->GetBodyId()) || !tileMapEditor) return;

    generatedRectangles = BuildRectangles(
        tileMapEditor->GetTileMap(), tileMapEditor->GetTileSet(),
        tileMapEditor->GetGrid(), generatedPolygons);
    if (generatedRectangles.empty() && generatedPolygons.empty()) return;
    Transform* transform = GetTransform();
    Transform* bodyTransform = rigidBody->GetTransform();
    if (!transform || !bodyTransform) return;

    const glm::mat4 worldMatrix = transform->GetWorldMatrix();
    const glm::vec2 bodyPosition = bodyTransform->GetWorldPosition();
    const float bodyAngle = -glm::radians(bodyTransform->GetWorldRotation());
    const float cosine = std::cos(bodyAngle);
    const float sine = std::sin(bodyAngle);
    const float pixelsPerMeter = EngineUtils::RenderUtils::PixelsPerMeter;

    b2ShapeDef definition = b2DefaultShapeDef();
    definition.enableContactEvents = true;
    definition.enableSensorEvents = true;
    definition.material.friction = friction;
    definition.material.restitution = bounciness;
    definition.material.rollingResistance = rollingResistance;
    definition.density = enabled ? density : 0.0f;
    definition.filter = enabled ? filter : b2Filter{};
    definition.isSensor = isSensor;

    auto createPolygon = [&](const glm::vec2* localPoints, int count) {
        b2Vec2 points[8];
        for (int index = 0; index < count; ++index) {
            const glm::vec2 world = glm::vec2(worldMatrix *
                glm::vec4(localPoints[index] + center, 0.0f, 1.0f));
            const glm::vec2 offset = world - bodyPosition;
            points[index] = {
                (offset.x * cosine - offset.y * sine) / pixelsPerMeter,
                (offset.x * sine + offset.y * cosine) / pixelsPerMeter
            };
        }
        const b2Hull hull = b2ComputeHull(points, count);
        if (hull.count < 3) return;
        const b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
        const b2ShapeId shape = b2CreatePolygonShape(rigidBody->GetBodyId(), &definition, &polygon);
        if (b2Shape_IsValid(shape)) shapeIds.push_back(shape);
    };

    for (const GeneratedRectangle& rectangle : generatedRectangles) {
        const glm::vec2 half = rectangle.size * 0.5f;
        const glm::vec2 localCorners[4] = {
            rectangle.center + glm::vec2{-half.x, -half.y},
            rectangle.center + glm::vec2{ half.x, -half.y},
            rectangle.center + glm::vec2{ half.x,  half.y},
            rectangle.center + glm::vec2{-half.x,  half.y}
        };
        createPolygon(localCorners, 4);
    }
    for (const GeneratedPolygon& polygon : generatedPolygons)
        createPolygon(polygon.points.data(), static_cast<int>(polygon.points.size()));
    ApplyMass();
}

void TileMapCollider::DestroyShapes()
{
    const b2BodyId bodyId = rigidBody ? rigidBody->GetBodyId() : b2_nullBodyId;
    for (b2ShapeId shape : shapeIds)
        if (b2Shape_IsValid(shape)) b2DestroyShape(shape, false);
    shapeIds.clear();
    generatedRectangles.clear();
    generatedPolygons.clear();
    if (b2Body_IsValid(bodyId)) b2Body_ApplyMassFromShapes(bodyId);
}

void TileMapCollider::ApplyMass()
{
    if (rigidBody && b2Body_IsValid(rigidBody->GetBodyId()))
        b2Body_ApplyMassFromShapes(rigidBody->GetBodyId());
}

void TileMapCollider::SetCenter(glm::vec2 value)
{
    if (center == value) return;
    center = value;
    MarkGeometryDirty();
    Notify(CENTER_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::SetDensity(float value)
{
    if (density == value) return;
    density = value;
    if (enabled) {
        for (b2ShapeId shape : shapeIds)
            if (b2Shape_IsValid(shape)) b2Shape_SetDensity(shape, density, false);
        ApplyMass();
    }
    Notify(DENSITY_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::SetBounciness(float value)
{
    if (bounciness == value) return;
    bounciness = value;
    for (b2ShapeId shape : shapeIds)
        if (b2Shape_IsValid(shape)) b2Shape_SetRestitution(shape, bounciness);
    Notify(BOUNCINESS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::SetIsSensor(bool value)
{
    if (isSensor == value) return;
    isSensor = value;
    MarkGeometryDirty();
    Notify(IS_SENSOR_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::SetFriction(float value)
{
    if (friction == value) return;
    friction = value;
    for (b2ShapeId shape : shapeIds)
        if (b2Shape_IsValid(shape)) b2Shape_SetFriction(shape, friction);
    Notify(FRICTION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::SetRollingResistance(float value)
{
    if (rollingResistance == value) return;
    rollingResistance = value;
    MarkGeometryDirty();
    Notify(ROLLING_RESISTANCE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void TileMapCollider::OnEnabled()
{
    if (state < State::PostInitialized) return;
    BindSources();
    geometryDirty = true;
    CreateShape();
    Component::OnEnabled();
}

void TileMapCollider::OnDisabled()
{
    for (b2ShapeId shape : shapeIds) {
        if (!b2Shape_IsValid(shape)) continue;
        b2Shape_SetFilter(shape, b2Filter{});
        b2Shape_SetDensity(shape, 0.0f, false);
    }
    ApplyMass();
    ClearSourceSubscriptions();
    Component::OnDisabled();
}

void TileMapCollider::OnTransformScaleUpdate()
{
    Transform* transform = GetTransform();
    if (!transform) return;
    const glm::vec2 worldScale = transform->GetWorldScale();
    if (cachedScale != worldScale) {
        cachedScale = worldScale;
        MarkGeometryDirty();
    }
}

void TileMapCollider::Shutdown()
{
    ClearSourceSubscriptions();
    DestroyShapes();
    shapeId = b2_nullShapeId;
    Component::Shutdown();
}

TileMapCollider* TileMapCollider::Copy()
{
    auto* copy = new TileMapCollider();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->center = center;
    copy->filter = filter;
    copy->isSensor = isSensor;
    copy->density = density;
    copy->friction = friction;
    copy->bounciness = bounciness;
    copy->rollingResistance = rollingResistance;
    copy->geometryDirty = true;
    copy->state = State::Loaded;
    return copy;
}

TileMapCollider* TileMapCollider::Copy(Container* targetContainer)
{
    TileMapCollider* copy = Copy();
    copy->Attach(targetContainer);
    return copy;
}

void TileMapCollider::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}
