#include "mcp/ToolSupport.hpp"

#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

#include <QJsonObject>

namespace mcp::support {

Container* ActiveContainer() {
    return Engine::Get()->GetActiveContainer();
}

Registry* ActiveRegistry() {
    Container* container = ActiveContainer();
    return container ? container->FindSystem<Registry>() : nullptr;
}

GameObject* FindGameObject(const std::string& id) {
    Registry* registry = ActiveRegistry();
    return registry ? registry->Find<GameObject>(id) : nullptr;
}

McpResult ResolveGameObject(const QJsonObject& params, GameObject** out) {
    const QString id = params.value("id").toString();
    if (id.isEmpty())
        return McpResult::Error(ObjectNotFound, "missing \"id\"");

    GameObject* object = FindGameObject(id.toStdString());
    if (!object)
        return McpResult::Error(ObjectNotFound, "no GameObject with id " + id);

    *out = object;
    return McpResult::Ok();
}

QString WorldMode() {
    Container* container = ActiveContainer();
    if (!container) return "None";
    switch (container->GetMode()) {
        case Container::Mode::Runtime: return "Runtime";
        case Container::Mode::Paused:  return "Paused";
        case Container::Mode::Editor:  return "Editor";
    }
    return "Unknown";
}

bool IsRuntimeWorld() {
    Container* container = ActiveContainer();
    if (!container) return false;
    return container->GetMode() != Container::Mode::Editor;
}

} // namespace mcp::support
