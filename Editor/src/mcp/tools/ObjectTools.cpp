#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/DestructiveImpact.hpp"
#include "mcp/PyApiCall.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/UserClarification.hpp"

#include "engine/components/Component.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

#include <QJsonObject>

namespace mcp {

namespace {

// GameObject's own handler is id-constructed exactly like a component handler, so the
// same property plumbing reaches name/tag/active without a special case.
McpResult GetObjectProperty(const QJsonObject& params, const char* property) {
    return pyapi::GetProperty(pyapi::kGameObject,
                              params.value("id").toString().toStdString(), property);
}

McpResult SetObjectProperty(const QJsonObject& params, const char* property) {
    if (!params.contains("value"))
        return McpResult::Error(ObjectNotFound, "missing \"value\"");
    return pyapi::SetProperty(pyapi::kGameObject,
                              params.value("id").toString().toStdString(), property,
                              params.value("value"));
}

} // namespace

void RegisterObjectTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("object.get_name", [](const QJsonObject& p) { return GetObjectProperty(p, "name"); });
    dispatcher.RegisterTool("object.set_name", [](const QJsonObject& p) { return SetObjectProperty(p, "name"); });
    dispatcher.RegisterTool("object.get_tag", [](const QJsonObject& p) { return GetObjectProperty(p, "tag"); });
    dispatcher.RegisterTool("object.set_tag", [](const QJsonObject& p) { return SetObjectProperty(p, "tag"); });
    dispatcher.RegisterTool("object.get_active", [](const QJsonObject& p) { return GetObjectProperty(p, "active"); });
    dispatcher.RegisterTool("object.set_active", [](const QJsonObject& p) { return SetObjectProperty(p, "active"); });

    // Reparenting. Transform.parent wants a Transform (or None), so it takes the same
    // wrapper path as an asset-valued property -- Transform is itself id-constructed.
    // Omit parentId to move the object to the scene root.
    dispatcher.RegisterTool("object.set_parent", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        const QString parentId = params.value("parentId").toString();
        if (!parentId.isEmpty()) {
            if (!support::FindGameObject(parentId.toStdString()))
                return McpResult::Error(ObjectNotFound, "no parent GameObject with id " + parentId);
            if (parentId == QString::fromStdString(object->GetID()))
                return McpResult::Error(WrongMode, "an object cannot be its own parent");
        }

        return pyapi::SetAssetProperty(pyapi::kTransform, object->GetID(), "parent",
                                       pyapi::kTransform, parentId.toStdString());
    });

    dispatcher.RegisterTool("object.get_parent", [](const QJsonObject& params) {
        return pyapi::GetProperty(pyapi::kTransform,
                                  params.value("id").toString().toStdString(), "parent");
    });

    // Direct C++: there is no remove_component binding to wrap, and this is structural
    // rather than a property read/write.
    dispatcher.RegisterTool("object.remove_component", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        const QString componentId = params.value("componentId").toString();
        if (componentId.isEmpty())
            return McpResult::Error(ObjectNotFound,
                "missing \"componentId\" -- get one from object.get_details");

        Registry* registry = support::ActiveRegistry();
        Component* component = registry ? registry->Find<Component>(componentId.toStdString()) : nullptr;
        if (!component)
            return McpResult::Error(ObjectNotFound, "no Component with id " + componentId);

        // Guard against detaching a component from the wrong object, which would
        // corrupt both objects' component lists.
        if (component->GetGameObject() != object)
            return McpResult::Error(WrongMode, "component " + componentId + " is not attached to " +
                                               QString::fromStdString(object->GetID()));

        const QString typeName = QString::fromStdString(component->GetTypeName());
        if (typeName == "Transform")
            return McpResult::Error(InvalidParams, "Transform is required and cannot be removed");

        ImpactClarification impact = AnalyzeComponentRemoval(object, component);
        const QString clarificationId = params.value("clarificationId").toString();
        if (clarificationId.isEmpty()) {
            const QString requestId = UserClarificationService::Get()->Create(
                std::move(impact.request));
            QJsonObject data{
                {"status", "clarification_required"},
                {"requestId", requestId},
                {"action", "remove_component"},
                {"componentId", componentId},
                {"type", typeName},
                {"affected", impact.affected},
                {"affectedTotal", impact.affectedTotal},
                {"truncated", impact.truncated}
            };
            return McpResult::Ok(data);
        }

        QJsonObject clarification;
        QString authorizationError;
        if (!UserClarificationService::Get()->Consume(
                clarificationId, impact.request.scope, QStringLiteral("remove"),
                &clarification, &authorizationError)) {
            return McpResult::Error(InvalidParams, authorizationError);
        }
        object->RemoveComponent(component);   // deferred destruction; fires REMOVE_COMPONENT_EVENT

        QJsonObject data;
        data["status"] = "removed";
        data["removed"] = componentId;
        data["type"] = typeName;
        data["clarification"] = clarification;
        data["affected"] = impact.affected;
        return McpResult::Ok(data);
    });
}

} // namespace mcp
