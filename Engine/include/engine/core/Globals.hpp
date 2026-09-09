#pragma once

#include "engine/core/System.hpp"
#include "engine/utils/Properties.hpp"

#include <glm/glm.hpp>

#include <string>
#include <variant>
#include <vector>

enum class GlobalValueType {
    Bool,
    Int,
    Float,
    String,
    Vector2,
    Vector3,
    Vector4,
    Color
};

using GlobalValue = std::variant<bool, int, float, std::string,
                                 glm::vec2, glm::vec3, glm::vec4>;

struct GlobalPropertyChange {
    std::string group;
    std::string property;
    GlobalValue previousValue;
    GlobalValue value;
};

struct GlobalProperty {
    std::string name;
    GlobalValueType type = GlobalValueType::Bool;
    GlobalValue value = false;

    // The same descriptor consumed by the Inspector's PropertyFactory. Keeping
    // it with the value lets the YAML choose sliders, ranges, dropdowns, etc.
    // without teaching the Settings panel a second widget vocabulary.
    Properties::PropDesc descriptor;
};

struct GlobalGroup {
    std::string name;
    std::vector<GlobalProperty> properties;
};

// Process-wide typed project settings loaded from one YAML file. Globals keeps
// the System serialization/event interface but is deliberately not attached to
// a Container and is not copied with editor/runtime worlds.
class Globals : public System {
public:
    static inline const Event PROPERTY_CHANGED_EVENT = Globals::CreateEvent();
    // Compatibility name for callers written against the original implementation.
    static inline const Event VALUES_CHANGED_EVENT = PROPERTY_CHANGED_EVENT;
    static constexpr const char* CONFIG_PATH = "Domain/lib/configs/Globals.config";

    static Globals* Get();

    void Init() override;
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    const std::vector<GlobalGroup>& GetGroups() const { return groups; }
    const GlobalProperty* FindProperty(const std::string& group,
                                       const std::string& property) const;

    template<typename T>
    T Get(const std::string& group, const std::string& property,
          T fallback = T{}) const
    {
        const GlobalProperty* found = FindProperty(group, property);
        if (!found) return fallback;
        const T* value = std::get_if<T>(&found->value);
        return value ? *value : fallback;
    }

    // Existing values can only be changed while the process is in editor mode
    // and its editor world is active. The replacement must retain the type
    // declared by the YAML schema.
    bool SetValue(const std::string& group, const std::string& property,
                  const GlobalValue& value);
    bool CanEdit() const;

    // Persistence is explicit so callers may group edits if needed.
    bool SaveConfig();

    std::string GetTypeName() const override { return "Globals"; }

private:
    Globals() = default;
    Globals(const Globals&) = delete;
    Globals& operator=(const Globals&) = delete;

    // Enforce the process-global ownership rule even if a caller reaches this
    // override through RuntimeObject and attempts to attach the singleton.
    void Attach(Container*) override {}

    static constexpr const char* LEGACY_CONFIG_PATH =
        "Domain/lib/configs/Gatekeeper.config";

    void LoadDefaults();
    bool LoadConfig();

    std::vector<GlobalGroup> groups;
};
