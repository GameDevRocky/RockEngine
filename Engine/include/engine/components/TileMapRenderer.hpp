#pragma once

#include <string>

#include "engine/components/Component.hpp"

class Container;
class TileMapEditor;

class TileMapRenderer : public Component {
public:
    enum class TileSizingMode { Native, FillCell };
    static inline const Event RENDER_CHANGED_EVENT = TileMapRenderer::CreateEvent();

    void Init() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    TileMapRenderer* Copy() override;
    TileMapRenderer* Copy(Container* container) override;
    void Accept(IVisitor* visitor) override;
    std::string GetTypeName() const override { return "TileMapRenderer"; }

    TileMapEditor* GetTileMapEditor();
    TileSizingMode GetTileSizingMode() const { return tileSizingMode; }
    void SetTileSizingMode(TileSizingMode value);
    const std::string& GetSortingLayer() const { return sortingLayer; }
    void SetSortingLayer(const std::string& value);
    int GetSortingOrder() const { return sortingOrder; }
    void SetSortingOrder(int value);

private:
    TileSizingMode tileSizingMode = TileSizingMode::Native;
    std::string sortingLayer = "Default";
    int sortingOrder = 0;
};
