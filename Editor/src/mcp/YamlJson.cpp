#include "mcp/YamlJson.hpp"

#include "yaml-cpp/yaml.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace mcp {

namespace {

// YAML scalars are untyped text. Recovering bool/number where possible keeps the JSON
// useful to a client (a position reads as a number, not "1.5"), with the raw string as
// the fallback.
QJsonValue ScalarToJson(const YAML::Node& node) {
    const std::string& raw = node.Scalar();
    if (raw == "true")  return true;
    if (raw == "false") return false;

    if (bool asBool = false; YAML::convert<bool>::decode(node, asBool))
        return asBool;
    if (double asDouble = 0.0; YAML::convert<double>::decode(node, asDouble))
        return asDouble;
    return QString::fromStdString(raw);
}

} // namespace

QJsonValue YamlToJson(const YAML::Node& node) {
    switch (node.Type()) {
        case YAML::NodeType::Null:
        case YAML::NodeType::Undefined:
            return QJsonValue();

        case YAML::NodeType::Scalar:
            return ScalarToJson(node);

        case YAML::NodeType::Sequence: {
            QJsonArray array;
            for (const YAML::Node& item : node)
                array.append(YamlToJson(item));
            return array;
        }

        case YAML::NodeType::Map: {
            QJsonObject object;
            for (const auto& entry : node)
                object[QString::fromStdString(entry.first.Scalar())] = YamlToJson(entry.second);
            return object;
        }
    }
    return QJsonValue();
}

} // namespace mcp
