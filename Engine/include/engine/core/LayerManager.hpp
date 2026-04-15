#pragma once
#include "engine/core/System.hpp"
#include <string>
#include <vector>

struct SortingLayer
{
    std::string name;
    int priority = 0;
};

class LayerManager : public System
{
public:
    LayerManager() = default;

    void Init() override;
    void Shutdown() override;

    LayerManager* Copy() override;
    LayerManager* Copy(Container* container) override;

    // Layer access
    const std::vector<SortingLayer>& GetLayers() const { return layers; }
    const SortingLayer* GetLayer(const std::string& name) const;
    int GetPriority(const std::string& name) const;

    // Layer names for GUI dropdowns
    std::vector<std::string> GetLayerNames() const;

    // Mutation
    void AddLayer(const std::string& name);
    void RemoveLayer(const std::string& name);
    void SetPriority(const std::string& name, int priority);

private:
    void SortLayers();

    std::vector<SortingLayer> layers;
};
