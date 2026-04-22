#include "engine/core/LayerManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/debug/Console.hpp"
#include "yaml-cpp/yaml.h"
#include <algorithm>

static constexpr const char* CONFIG_PATH = "Domain/lib/configs/SortingLayerConfig.yaml";

void LayerManager::Init()
{
    layers.clear();

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(CONFIG_PATH);
    }
    catch (const YAML::Exception& e)
    {
        //Console::Alert(std::string("LayerManager: failed to load config: ") + e.what());
        layers.push_back({ "Default", 0 });
        return;
    }

    const YAML::Node& list = root["SortingLayers"];
    if (!list || !list.IsSequence())
    {
        Console::Alert("LayerManager: 'SortingLayers' key missing in config");
        layers.push_back({ "Default", 0 });
        return;
    }

    for (const auto& entry : list)
    {
        SortingLayer layer;
        layer.name     = entry["name"].as<std::string>("Unnamed");
        layer.priority = entry["priority"].as<int>(0);
        layers.push_back(layer);
    }

    SortLayers();
}

void LayerManager::Shutdown()
{
    layers.clear();
}

LayerManager* LayerManager::Copy()
{
    auto* copy = new LayerManager();
    copy->layers = layers;
    return copy;
}

LayerManager* LayerManager::Copy(Container* container)
{
    auto* copy = Copy();
    copy->Attach(container);
    return copy;
}

const SortingLayer* LayerManager::GetLayer(const std::string& name) const
{
    for (const auto& layer : layers)
        if (layer.name == name) return &layer;
    return nullptr;
}

int LayerManager::GetPriority(const std::string& name) const
{
    const SortingLayer* layer = GetLayer(name);
    if (layer) return layer->priority;

    // Unknown name → treat as Default
    const SortingLayer* def = GetLayer("Default");
    return def ? def->priority : 0;
}

std::vector<std::string> LayerManager::GetLayerNames() const
{
    std::vector<std::string> names;
    names.reserve(layers.size());
    for (const auto& layer : layers)
        names.push_back(layer.name);
    return names;
}

void LayerManager::AddLayer(const std::string& name)
{
    if (GetLayer(name)) return;

    int maxPriority = layers.empty() ? -1 : layers.back().priority;
    layers.push_back({ name, maxPriority + 1 });
}

void LayerManager::RemoveLayer(const std::string& name)
{
    layers.erase(
        std::remove_if(layers.begin(), layers.end(),
                       [&name](const SortingLayer& l) { return l.name == name; }),
        layers.end());
}

void LayerManager::SetPriority(const std::string& name, int priority)
{
    for (auto& layer : layers)
    {
        if (layer.name == name)
        {
            layer.priority = priority;
            SortLayers();
            return;
        }
    }
}

void LayerManager::SortLayers()
{
    std::stable_sort(layers.begin(), layers.end(),
                     [](const SortingLayer& a, const SortingLayer& b)
                     { return a.priority < b.priority; });
}
