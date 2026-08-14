#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/YamlJson.hpp"

#include "engine/components/Component.hpp"
#include "engine/core/GameObject.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace mcp {

// The fallback for "show me everything about this object" -- including fields no
// hand-written tool exposes yet.
void RegisterDebugTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("object.dump_state", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        QJsonArray components;
        for (Component* component : object->GetAllComponents()) {
            QJsonObject entry;
            entry["type"] = QString::fromStdString(component->GetTypeName());
            entry["state"] = YamlToJson(component->Serialize());
            components.append(entry);
        }

        QJsonObject data;
        data["gameobject"] = YamlToJson(object->Serialize());
        data["components"] = components;
        return McpResult::Ok(data);
    });
}

} // namespace mcp
