#include "engine/components/TileMapEditor.hpp"

#include <array>
#include <cmath>

#include "Engine.hpp"

#include "engine/components/Grid.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
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
    registry = container->FindSystem<Registry>();
    if (!GetComponent<TileMapRenderer>()) RequireComponent<TileMapRenderer>();
}

void TileMapEditor::PostInit()
{
    if (state >= State::PostInitialized) return;
    Component::PostInit();
    RefreshGrid();
    if (enabled) AcquireTileMap();
}

void TileMapEditor::Shutdown()
{
    ClearGridSubscriptions();
    gridId.clear();
    ReleaseTileMap();
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
    state = State::Loaded;
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
    Grid* grid = registry ? registry->Find<Grid>(gridId) : nullptr;
    return grid && !grid->IsMarkedForDestroy() ? grid : nullptr;
}

void TileMapEditor::RefreshGrid(const std::string& removedGridId)
{
    ClearGridSubscriptions();
    if (!registry || IsMarkedForDestroy()) return;

    std::string nextGridId;
    GameObject* owner = registry->Find<GameObject>(gameobject_id);
    Transform* transform = owner ? owner->GetTransform() : nullptr;
    if (transform) {
        SubscribeToGridHierarchy(transform, Transform::PARENT_CHANGED_EVENT);
        for (Transform* parent = transform->GetParent(); parent; parent = parent->GetParent()) {
            // An intermediate parent can move without changing this object's direct parent.
            SubscribeToGridHierarchy(parent, Transform::PARENT_CHANGED_EVENT);
            GameObject* parentObject = parent->GetGameObject();
            if (!parentObject || parentObject->IsMarkedForDestroy()) continue;
            SubscribeToGridHierarchy(parentObject, GameObject::ADD_COMPONENT_EVENT);
            SubscribeToGridHierarchy(parentObject, GameObject::REMOVE_COMPONENT_EVENT);
            Grid* grid = parentObject->GetComponent<Grid>();
            if (nextGridId.empty() && grid && !grid->IsMarkedForDestroy()
                && grid->GetID() != removedGridId) {
                nextGridId = grid->GetID();
                SubscribeToGridHierarchy(grid, RuntimeObject::SHUTDOWN_EVENT);
            }
        }
    }

    if (gridId == nextGridId) return;
    gridId = nextGridId;
    Notify(GRID_CHANGED_EVENT, gridId);
    if (GameObject* object = registry->Find<GameObject>(gameobject_id)) {
        if (TileMapRenderer* renderer = object->GetComponent<TileMapRenderer>())
            renderer->Notify(TileMapRenderer::RENDER_CHANGED_EVENT);
    }
}

void TileMapEditor::SubscribeToGridHierarchy(RuntimeObject* source, Event event)
{
    const std::string sourceId = source->GetID();
    const int subscriptionId = source->Subscribe(
        [componentId = GetID(), containerId = container->GetID(), sourceId, event]() {
            // The editor and play-world copies share component IDs, but not container IDs.
            for (Container* world : {Engine::Get()->GetEditorContainer(), Engine::Get()->GetRuntimeContainer()}) {
                if (!world || world->GetID() != containerId) continue;
                Registry* registry = world->FindSystem<Registry>();
                TileMapEditor* editor = registry ? registry->Find<TileMapEditor>(componentId) : nullptr;
                if (!editor || editor->IsMarkedForDestroy()) return false;
                editor->RefreshGrid(event == RuntimeObject::SHUTDOWN_EVENT ? sourceId : std::string{});
                return event != RuntimeObject::SHUTDOWN_EVENT;
            }
            return false;
        }, event);
    gridSubscriptions.push_back({sourceId, subscriptionId});
}

void TileMapEditor::ClearGridSubscriptions()
{
    if (registry) {
        for (const GridSubscription& subscription : gridSubscriptions) {
            if (RuntimeObject* source = registry->Find<RuntimeObject>(subscription.sourceId))
                source->Unsubscribe(subscription.subscriptionId);
        }
    }
    gridSubscriptions.clear();
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
    const std::string id = map ? map->GetID() : std::string{};
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

TileId TileMapEditor::GetTileId(const glm::ivec2& cell) const
{
    TileMap* map = GetTileMap();
    if (map == nullptr) return EMPTY_TILE_ID;
    return map->GetTileId(cell);
}

bool TileMapEditor::SetTileId(const glm::ivec2& cell, TileId tileId)
{
    TileMap* map = GetTileMap();
    if (map == nullptr) return false;
    return map->SetTileId(cell, tileId);
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
        neighbors[i] = GetTileId(cell + offsets[i]);
    return neighbors;
}

glm::ivec2 TileMapEditor::WorldToCell(const glm::vec2& world) const
{
    Grid* grid = GetGrid();
    GameObject* owner = registry ? registry->Find<GameObject>(gameobject_id) : nullptr;
    Transform* transform = owner ? owner->GetTransform() : nullptr;
    if (!grid || !transform) return {0, 0};

    const glm::mat4 worldMatrix = transform->GetWorldMatrix();
    const glm::vec2 stride = grid->GetSize() + grid->GetGap();
    if (std::abs(glm::determinant(worldMatrix)) <= 1.0e-6f
        || std::abs(stride.x) <= 1.0e-6f || std::abs(stride.y) <= 1.0e-6f)
        return {0, 0};

    const glm::vec2 local = glm::vec2(glm::inverse(worldMatrix) * glm::vec4(world, 0.0f, 1.0f));
    return {static_cast<int>(std::floor(local.x / stride.x)),
            static_cast<int>(std::floor(local.y / stride.y))};
}

glm::vec2 TileMapEditor::CellToWorld(const glm::ivec2& cell) const
{
    Grid* grid = GetGrid();
    GameObject* owner = registry ? registry->Find<GameObject>(gameobject_id) : nullptr;
    Transform* transform = owner ? owner->GetTransform() : nullptr;
    if (!grid || !transform) return {0.0f, 0.0f};

    const glm::vec2 local = glm::vec2(cell) * (grid->GetSize() + grid->GetGap());
    return glm::vec2(transform->GetWorldMatrix() * glm::vec4(local, 0.0f, 1.0f));
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

