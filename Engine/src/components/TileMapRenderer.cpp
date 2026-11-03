#include "engine/components/TileMapRenderer.hpp"

#include "engine/components/TileMapEditor.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/utils/IVisitor.hpp"

void TileMapRenderer::Init()
{
    if (state >= State::Initialized) return;
    Component::Init();
    if (!GetComponent<TileMapEditor>()) RequireComponent<TileMapEditor>();
}

YAML::Node TileMapRenderer::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["materialId"] = materialId;
    node["tileSizingMode"] = tileSizingMode == TileSizingMode::FillCell ? "FillCell" : "Native";
    node["sortingLayer"] = sortingLayer;
    node["sortingOrder"] = sortingOrder;
    return node;
}

void TileMapRenderer::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    materialId = node["materialId"].as<std::string>("m1");
    tileSizingMode = node["tileSizingMode"].as<std::string>("Native") == "FillCell"
        ? TileSizingMode::FillCell : TileSizingMode::Native;
    sortingLayer = node["sortingLayer"].as<std::string>("Default");
    sortingOrder = node["sortingOrder"].as<int>(0);
    state = State::Loaded;
}

TileMapRenderer* TileMapRenderer::Copy()
{
    auto* copy = new TileMapRenderer();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->materialId = materialId;
    copy->tileSizingMode = tileSizingMode;
    copy->sortingLayer = sortingLayer;
    copy->sortingOrder = sortingOrder;
    return copy;
}

TileMapRenderer* TileMapRenderer::Copy(Container* targetContainer)
{
    TileMapRenderer* copy = Copy();
    copy->Attach(targetContainer);
    return copy;
}

void TileMapRenderer::Accept(IVisitor* visitor)
{
    visitor->Visit(this);
}

TileMapEditor* TileMapRenderer::GetTileMapEditor()
{
    GameObject* owner = GetGameObject();
    return owner ? owner->GetComponent<TileMapEditor>() : nullptr;
}

Grid* TileMapRenderer::GetGrid()
{
    TileMapEditor* editor = GetTileMapEditor();
    return editor ? editor->GetGrid() : nullptr;
}

Material* TileMapRenderer::GetMaterial() const
{
    return AssetManager::Get().GetMaterial(materialId);
}

void TileMapRenderer::SetMaterial(const std::string& id)
{
    if (materialId == id) return;
    materialId = id;
    Notify(RENDER_CHANGED_EVENT);
}

void TileMapRenderer::SetTileSizingMode(TileSizingMode value)
{
    if (tileSizingMode == value) return;
    tileSizingMode = value;
    Notify(RENDER_CHANGED_EVENT);
}

void TileMapRenderer::SetSortingLayer(const std::string& value)
{
    if (sortingLayer == value) return;
    sortingLayer = value;
    Notify(RENDER_CHANGED_EVENT);
}

void TileMapRenderer::SetSortingOrder(int value)
{
    if (sortingOrder == value) return;
    sortingOrder = value;
    Notify(RENDER_CHANGED_EVENT);
}
