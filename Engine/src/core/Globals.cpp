#include "engine/core/Globals.hpp"

#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace {

using Properties::PropDesc;
using Properties::Tags;

GlobalProperty* FindProperty(std::vector<GlobalGroup>& groups,
                             const std::string& groupName,
                             const std::string& propertyName)
{
    for (GlobalGroup& group : groups) {
        if (group.name != groupName) continue;
        for (GlobalProperty& property : group.properties)
            if (property.name == propertyName) return &property;
        return nullptr;
    }
    return nullptr;
}

const GlobalProperty* FindProperty(const std::vector<GlobalGroup>& groups,
                                   const std::string& groupName,
                                   const std::string& propertyName)
{
    for (const GlobalGroup& group : groups) {
        if (group.name != groupName) continue;
        for (const GlobalProperty& property : group.properties)
            if (property.name == propertyName) return &property;
        return nullptr;
    }
    return nullptr;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

const char* TypeName(GlobalValueType type)
{
    switch (type) {
        case GlobalValueType::Bool:    return "bool";
        case GlobalValueType::Int:     return "int";
        case GlobalValueType::Float:   return "float";
        case GlobalValueType::String:  return "string";
        case GlobalValueType::Vector2: return "vec2";
        case GlobalValueType::Vector3: return "vec3";
        case GlobalValueType::Vector4: return "vec4";
        case GlobalValueType::Color:   return "color";
    }
    return "bool";
}

bool ParseType(const std::string& text, GlobalValueType& type)
{
    const std::string name = Lower(text);
    if (name == "bool" || name == "boolean") type = GlobalValueType::Bool;
    else if (name == "int" || name == "integer") type = GlobalValueType::Int;
    else if (name == "float" || name == "number") type = GlobalValueType::Float;
    else if (name == "string" || name == "text") type = GlobalValueType::String;
    else if (name == "vec2" || name == "vector2") type = GlobalValueType::Vector2;
    else if (name == "vec3" || name == "vector3") type = GlobalValueType::Vector3;
    else if (name == "vec4" || name == "vector4") type = GlobalValueType::Vector4;
    else if (name == "color" || name == "colour") type = GlobalValueType::Color;
    else return false;
    return true;
}

bool ParseInteger(const std::string& text, int& value)
{
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end != text.c_str() + text.size() ||
        parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool ParseFloat(const std::string& text, float& value)
{
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno != 0 || end != text.c_str() + text.size() || !std::isfinite(parsed))
        return false;
    value = parsed;
    return true;
}

bool InferType(const YAML::Node& value, GlobalValueType& type)
{
    if (value.IsSequence()) {
        if (value.size() == 2) type = GlobalValueType::Vector2;
        else if (value.size() == 3) type = GlobalValueType::Vector3;
        else if (value.size() == 4) type = GlobalValueType::Vector4;
        else return false;
        return true;
    }
    if (!value.IsScalar()) return false;

    const std::string scalar = value.Scalar();
    const std::string lower = Lower(scalar);
    if (lower == "true" || lower == "false") {
        type = GlobalValueType::Bool;
        return true;
    }
    int integer = 0;
    if (ParseInteger(scalar, integer)) {
        type = GlobalValueType::Int;
        return true;
    }
    float number = 0.0f;
    if (ParseFloat(scalar, number)) {
        type = GlobalValueType::Float;
        return true;
    }
    type = GlobalValueType::String;
    return true;
}

template<int N>
bool ReadVector(const YAML::Node& node, glm::vec<N, float>& result)
{
    if (!node.IsSequence() || node.size() != N) return false;
    try {
        for (int i = 0; i < N; ++i) result[i] = node[i].as<float>();
    } catch (const YAML::Exception&) {
        return false;
    }
    return true;
}

bool ReadValue(const YAML::Node& node, GlobalValueType type, GlobalValue& value)
{
    try {
        switch (type) {
            case GlobalValueType::Bool:
                value = node.as<bool>();
                return true;
            case GlobalValueType::Int:
                value = node.as<int>();
                return true;
            case GlobalValueType::Float:
                value = node.as<float>();
                return true;
            case GlobalValueType::String:
                value = node.as<std::string>();
                return true;
            case GlobalValueType::Vector2: {
                glm::vec2 vector(0.0f);
                if (!ReadVector<2>(node, vector)) return false;
                value = vector;
                return true;
            }
            case GlobalValueType::Vector3: {
                glm::vec3 vector(0.0f);
                if (!ReadVector<3>(node, vector)) return false;
                value = vector;
                return true;
            }
            case GlobalValueType::Vector4:
            case GlobalValueType::Color: {
                glm::vec4 vector(0.0f);
                if (!ReadVector<4>(node, vector)) return false;
                value = vector;
                return true;
            }
        }
    } catch (const YAML::Exception&) {
    }
    return false;
}

YAML::Node WriteValue(const GlobalValue& value)
{
    YAML::Node node;
    std::visit([&node](const auto& typedValue) {
        using T = std::decay_t<decltype(typedValue)>;
        if constexpr (std::is_same_v<T, glm::vec2> ||
                      std::is_same_v<T, glm::vec3> ||
                      std::is_same_v<T, glm::vec4>) {
            for (glm::length_t i = 0; i < typedValue.length(); ++i)
                node.push_back(typedValue[i]);
            node.SetStyle(YAML::EmitterStyle::Flow);
        } else {
            node = typedValue;
        }
    }, value);
    return node;
}

Tags DefaultTag(GlobalValueType type)
{
    switch (type) {
        case GlobalValueType::Bool:    return Tags::TOGGLE;
        case GlobalValueType::Int:     return Tags::INT;
        case GlobalValueType::Float:   return Tags::FLOAT;
        case GlobalValueType::String:  return Tags::STRING;
        case GlobalValueType::Vector2: return Tags::VECTOR2;
        case GlobalValueType::Vector3: return Tags::VECTOR3;
        case GlobalValueType::Vector4: return Tags::VECTOR4;
        case GlobalValueType::Color:   return Tags::COLOR;
    }
    return Tags::NONE;
}

std::string WidgetName(const GlobalProperty& property)
{
    switch (property.descriptor.tag) {
        case Tags::SLIDER:       return "slider";
        case Tags::RANGE_SLIDER: return "range_slider";
        case Tags::MULTILINE:    return "multiline";
        case Tags::DROPDOWN:     return "dropdown";
        default:                 return {};
    }
}

void ReadDescriptor(const YAML::Node& schema, GlobalProperty& property)
{
    PropDesc& descriptor = property.descriptor;
    descriptor.tag = DefaultTag(property.type);
    descriptor.step = property.type == GlobalValueType::Int ? 1.0f : 0.1f;

    if (const YAML::Node description = schema["description"])
        descriptor.description = description.as<std::string>("");
    if (const YAML::Node readOnly = schema["readOnly"])
        descriptor.readOnly = readOnly.as<bool>(false);
    if (const YAML::Node step = schema["step"])
        descriptor.step = step.as<float>(descriptor.step);

    const YAML::Node range = schema["range"];
    if (range && range.IsSequence() && range.size() == 2) {
        descriptor.min = range[0].as<float>(descriptor.min);
        descriptor.max = range[1].as<float>(descriptor.max);
    } else if (schema["min"] && schema["max"]) {
        descriptor.min = schema["min"].as<float>(descriptor.min);
        descriptor.max = schema["max"].as<float>(descriptor.max);
    }

    const std::string widget = Lower(schema["widget"].as<std::string>(""));
    if (widget == "slider" && property.type == GlobalValueType::Float)
        descriptor.tag = Tags::SLIDER;
    else if ((widget == "range_slider" || widget == "rangeslider") &&
             property.type == GlobalValueType::Vector2)
        descriptor.tag = Tags::RANGE_SLIDER;
    else if ((widget == "multiline" || widget == "text_box" || widget == "textbox") &&
             property.type == GlobalValueType::String)
        descriptor.tag = Tags::MULTILINE;

    const YAML::Node options = schema["options"];
    if (!options || !options.IsSequence()) return;

    if (property.type == GlobalValueType::Int) {
        for (const YAML::Node& option : options) {
            try {
                if (option.IsMap()) {
                    const int value = option["value"].as<int>();
                    const std::string label = option["label"].as<std::string>(
                        std::to_string(value));
                    descriptor.dropdownOptions.push_back({ label, value });
                } else {
                    const int value = option.as<int>();
                    descriptor.dropdownOptions.push_back({ std::to_string(value), value });
                }
            } catch (const YAML::Exception&) {
            }
        }
    } else if (property.type == GlobalValueType::String) {
        for (const YAML::Node& option : options) {
            try {
                const std::string label = option.IsMap()
                    ? option["label"].as<std::string>()
                    : option.as<std::string>();
                descriptor.dropdownOptions.push_back({ label, label });
            } catch (const YAML::Exception&) {
            }
        }
    }

    if (!descriptor.dropdownOptions.empty() &&
        (property.type == GlobalValueType::Int ||
         property.type == GlobalValueType::String))
        descriptor.tag = Tags::DROPDOWN;
}

YAML::Node WriteDescriptor(const GlobalProperty& property)
{
    YAML::Node schema(YAML::NodeType::Map);
    schema["type"] = TypeName(property.type);
    schema["value"] = WriteValue(property.value);

    const std::string widget = WidgetName(property);
    if (!widget.empty()) schema["widget"] = widget;
    if (!property.descriptor.description.empty())
        schema["description"] = property.descriptor.description;
    if (property.descriptor.readOnly)
        schema["readOnly"] = true;

    const bool hasRange = property.descriptor.min > -std::numeric_limits<float>::max() &&
                          property.descriptor.max <  std::numeric_limits<float>::max() &&
                          property.descriptor.max > property.descriptor.min;
    if (hasRange) {
        YAML::Node range;
        range.push_back(property.descriptor.min);
        range.push_back(property.descriptor.max);
        range.SetStyle(YAML::EmitterStyle::Flow);
        schema["range"] = range;
    }

    const float defaultStep = property.type == GlobalValueType::Int ? 1.0f : 0.1f;
    if (property.descriptor.step != defaultStep)
        schema["step"] = property.descriptor.step;

    if (!property.descriptor.dropdownOptions.empty()) {
        YAML::Node options(YAML::NodeType::Sequence);
        for (const auto& [label, value] : property.descriptor.dropdownOptions) {
            if (property.type == GlobalValueType::Int) {
                const int integer = std::any_cast<int>(value);
                YAML::Node option(YAML::NodeType::Map);
                option["label"] = label;
                option["value"] = integer;
                options.push_back(option);
            } else {
                options.push_back(label);
            }
        }
        schema["options"] = options;
    }
    return schema;
}

bool ReadProperty(const std::string& name, const YAML::Node& node,
                  GlobalProperty& property)
{
    property = GlobalProperty{};
    property.name = name;

    // Legacy Gatekeeper entries were scalar booleans. Scalars and sequences are
    // also convenient shorthand for new Globals when no editor metadata is needed.
    const YAML::Node valueNode = node.IsMap() ? node["value"] : node;
    if (!valueNode) return false;

    if (node.IsMap() && node["type"]) {
        if (!ParseType(node["type"].as<std::string>(""), property.type)) return false;
    } else if (!InferType(valueNode, property.type)) {
        return false;
    }

    if (!ReadValue(valueNode, property.type, property.value)) return false;
    if (node.IsMap()) ReadDescriptor(node, property);
    else property.descriptor.tag = DefaultTag(property.type);
    return true;
}

} // namespace

Globals* Globals::Get()
{
    static Globals* instance = []() {
        auto* globals = new Globals();
        globals->Init();
        return globals;
    }();
    return instance;
}

void Globals::Init()
{
    if (state >= State::Initialized) return;
    if (!LoadConfig()) LoadDefaults();
    RuntimeObject::Init();
}

void Globals::LoadDefaults()
{
    GlobalProperty accelerated;
    accelerated.name = "useGPUNormalMapGeneration";
    accelerated.type = GlobalValueType::Bool;
    accelerated.value = false;
    accelerated.descriptor.Tag(Tags::TOGGLE)
        .Desc("Generate texture normal maps with an OpenGL compute shader. Falls back to CPU when unavailable.");
    groups = { { "Rendering", { std::move(accelerated) } } };
}

bool Globals::LoadConfig()
{
    std::string path = EngineUtils::GetAssetPath(CONFIG_PATH);
    std::error_code pathError;
    if (!std::filesystem::exists(path, pathError)) {
        const std::string legacyPath = EngineUtils::GetAssetPath(LEGACY_CONFIG_PATH);
        if (std::filesystem::exists(legacyPath, pathError)) path = legacyPath;
    }
    try {
        Deserialize(YAML::LoadFile(path));
    } catch (const std::exception& e) {
        Console::Warn("Globals: failed to load " + path + ": " + e.what());
        return false;
    }

    if (groups.empty()) {
        Console::Warn("Globals: no valid properties found in " + path);
        return false;
    }
    return true;
}

YAML::Node Globals::Serialize()
{
    YAML::Node root;
    YAML::Node globals(YAML::NodeType::Map);
    for (const GlobalGroup& group : groups) {
        YAML::Node properties(YAML::NodeType::Map);
        for (const GlobalProperty& property : group.properties)
            properties[property.name] = WriteDescriptor(property);
        globals[group.name] = properties;
    }
    root["Globals"] = globals;
    return root;
}

void Globals::Deserialize(const YAML::Node& node)
{
    // Serializable makes this public, but an initialized runtime copy remains
    // immutable even when a caller retains its pointer.
    if (state >= State::Initialized && !CanEdit()) return;

    groups.clear();
    const YAML::Node canonical = node["Globals"];
    const YAML::Node legacy = node["Gatekeeper"];
    // Keep these as separate handles. Assigning a value through yaml-cpp's
    // undefined subscript proxy attempts to mutate that missing key instead of
    // simply rebinding the Node handle.
    const YAML::Node globals = canonical.IsDefined() ? canonical : legacy;
    if (!globals.IsDefined() || !globals.IsMap()) return;

    for (const auto& groupEntry : globals) {
        GlobalGroup group;
        try {
            group.name = groupEntry.first.as<std::string>();
        } catch (const YAML::Exception&) {
            continue;
        }

        const YAML::Node propertyMap = groupEntry.second;
        if (!propertyMap.IsMap()) continue;
        for (const auto& propertyEntry : propertyMap) {
            try {
                GlobalProperty property;
                const std::string name = propertyEntry.first.as<std::string>();
                if (ReadProperty(name, propertyEntry.second, property))
                    group.properties.push_back(std::move(property));
            } catch (const YAML::Exception&) {
                // A malformed property is isolated; other project globals still load.
            }
        }
        if (!group.properties.empty()) groups.push_back(std::move(group));
    }

    state = State::Loaded;
}

const GlobalProperty* Globals::FindProperty(const std::string& group,
                                            const std::string& property) const
{
    return ::FindProperty(groups, group, property);
}

bool Globals::CanEdit() const
{
    Engine* engine = Engine::Get();
    if (!engine->IsEditor()) return false;
    Container* active = engine->GetActiveContainer();
    return !active || active->GetMode() == Container::Mode::Editor;
}

bool Globals::SetValue(const std::string& group, const std::string& property,
                       const GlobalValue& value)
{
    if (!CanEdit()) return false;
    GlobalProperty* found = ::FindProperty(groups, group, property);
    if (!found || found->value.index() != value.index()) return false;
    if (found->value == value) return true;

    GlobalPropertyChange change{ group, property, found->value, value };
    found->value = value;
    Notify(PROPERTY_CHANGED_EVENT, change);
    return true;
}

bool Globals::SaveConfig()
{
    if (!CanEdit()) return false;

    const std::filesystem::path path(EngineUtils::GetAssetPath(CONFIG_PATH));
    std::error_code ec;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        Console::Alert("Globals: cannot create config directory: " + ec.message());
        return false;
    }

    YAML::Emitter emitter;
    emitter << Serialize();
    if (!emitter.good()) {
        Console::Alert(std::string("Globals: failed to serialize config: ") +
                       emitter.GetLastError());
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        Console::Alert("Globals: cannot write " + path.string());
        return false;
    }
    output << emitter.c_str() << '\n';
    if (!output.good()) {
        Console::Alert("Globals: failed while writing " + path.string());
        return false;
    }
    return true;
}
