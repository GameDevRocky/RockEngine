#pragma once
#include "engine/core/System.hpp"
#include <string>
#include <vector>

class TagManager : public System {
public:
    TagManager() = default;

    void Init() override;
    void Shutdown() override;
    TagManager* Copy() override;
    TagManager* Copy(Container* container) override;

    const std::vector<std::string>& GetTags() const { return tags; }
    bool HasTag(const std::string& tag) const;
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);

private:
    std::vector<std::string> tags;
};