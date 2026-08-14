#pragma once

#include <QJsonValue>

namespace YAML { class Node; }

namespace mcp {

// Serialize() output -> JSON, for the generic object-state dump.
//
// Every Serializable already emits its complete field set as YAML, which makes it the
// one genuinely type-agnostic view of an object -- the alternative (IVisitor) is
// hand-written per type and Qt-coupled. Read-only: Deserialize() consumes a whole node
// rather than merging, so round-tripping a patched node is not a safe generic setter.
QJsonValue YamlToJson(const YAML::Node& node);

} // namespace mcp
