#include "engine/core/TagManager.hpp"
#include "engine/core/Container.hpp"
#include "engine/debug/Console.hpp"
#include "yaml-cpp/yaml.h"
#include <algorithm>

static constexpr const char* CONFIG_PATH = "Domain/lib/configs/TagConfig.config";

void TagManager::Init()
{
    tags.clear();

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(CONFIG_PATH);
    }
    catch (const YAML::Exception&)
    {
        tags.push_back("Untagged");
        return;
    }

    const YAML::Node& list = root["Tags"];
    if (!list || !list.IsSequence())
    {
        Console::Alert("TagManager: 'Tags' key missing in config");
        tags.push_back("Untagged");
        return;
    }

    for (const auto& entry : list)
        tags.push_back(entry.as<std::string>("Untagged"));
}

void TagManager::Shutdown()
{
    tags.clear();
}

TagManager* TagManager::Copy()
{
    auto* copy = new TagManager();
    copy->tags = tags;
    return copy;
}

TagManager* TagManager::Copy(Container* container)
{
    auto* copy = Copy();
    copy->Attach(container);
    return copy;
}

bool TagManager::HasTag(const std::string& tag) const
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

void TagManager::AddTag(const std::string& tag)
{
    if (HasTag(tag)) return;
    tags.push_back(tag);
}

void TagManager::RemoveTag(const std::string& tag)
{
    tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());
}
