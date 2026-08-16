#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/DestructiveImpact.hpp"
#include "mcp/PyBind.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/UserClarification.hpp"

#include "engine/components/Component.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"

#include <QJsonObject>

#include <algorithm>

namespace py = pybind11;

namespace mcp {

namespace {

// These go straight to rock_engine rather than through Domain/lib/api, which is the
// documented exception to the "wrap the handlers" rule: the handler layer has no
// instantiate/duplicate of its own (GameObject.destroy() just forwards to the same
// binding), so there is no id-resolution or coercion above these to reuse.
constexpr const char* kGameObjectModule = "rock_engine.core.gameobject_module";

// Like every other mutating tool here, these bypass UndoSystem -- an MCP edit is
// another engine-driven caller, same category as a script. See PyApiCall.hpp.
//
// Takes std::strings, not py::objects: constructing a py::str allocates a Python object,
// which is only legal while the GIL is held. Building the arguments at the call site --
// outside this function -- corrupts the interpreter and takes the editor down with it.
McpResult CallGameObjectBinding(const char* function, const std::vector<std::string>& args) {
    py::gil_scoped_acquire gil;
    try {
        py::module_ module = py::module_::import(kGameObjectModule);
        py::tuple callArgs(args.size());
        for (std::size_t i = 0; i < args.size(); ++i)
            callArgs[i] = py::str(args[i]);
        py::object returned = module.attr(function)(*callArgs);

        if (returned.is_none())
            return McpResult::Ok();
        return McpResult::Ok(QString::fromStdString(py::str(returned).cast<std::string>()));
    } catch (const py::error_already_set& e) {
        return McpResult::Error(PythonError, QString::fromUtf8(e.what()));
    }
}

McpResult ResolveDestinationScene(const QJsonObject& params, Scene** out) {
    const QString siblingId = params.value("id").toString();
    const QString requestedSceneId = params.value("sceneId").toString();

    if (!siblingId.isEmpty()) {
        GameObject* sibling = support::FindGameObject(siblingId.toStdString());
        if (!sibling)
            return McpResult::Error(ObjectNotFound,
                                    "no GameObject with id " + siblingId);

        Scene* scene = sibling->GetScene();
        if (!scene)
            return McpResult::Error(ObjectNotFound,
                                    "no scene for GameObject " + siblingId);

        if (!requestedSceneId.isEmpty() &&
            requestedSceneId != QString::fromStdString(scene->GetID())) {
            return McpResult::Error(WrongMode,
                "sibling " + siblingId + " does not belong to scene " + requestedSceneId);
        }

        *out = scene;
        return McpResult::Ok();
    }

    Container* container = support::ActiveContainer();
    SceneManager* sceneManager = container ? container->FindSystem<SceneManager>() : nullptr;
    if (!sceneManager)
        return McpResult::Error(WrongMode,
                                "no SceneManager on the active container");

    const std::vector<Scene*> scenes = sceneManager->GetScenes();
    if (requestedSceneId.isEmpty()) {
        if (scenes.size() != 1) {
            return McpResult::Error(ObjectNotFound,
                "pass \"scene_id\" -- there is not exactly one loaded scene to default to");
        }
        *out = scenes.front();
        return McpResult::Ok();
    }

    const auto match = std::find_if(scenes.begin(), scenes.end(),
        [&requestedSceneId](Scene* scene) {
            return scene && QString::fromStdString(scene->GetID()) == requestedSceneId;
        });
    if (match == scenes.end())
        return McpResult::Error(ObjectNotFound,
                                "no loaded scene with id " + requestedSceneId);

    *out = *match;
    return McpResult::Ok();
}

} // namespace

void RegisterLifecycleTools(McpDispatcher& dispatcher) {
    // A sibling remains the most precise placement hint, but it cannot bootstrap an
    // empty scene. In that case sceneId selects the destination, defaulting to the sole
    // loaded scene when unambiguous.
    dispatcher.RegisterTool("object.instantiate", [](const QJsonObject& params) {
        Scene* scene = nullptr;
        if (McpResult resolved = ResolveDestinationScene(params, &scene); !resolved.ok)
            return resolved;

        const QString name = params.value("name").toString(QStringLiteral("GameObject"));
        auto* object = new GameObject();
        object->SetName(name.toStdString());
        scene->AddGameObject(object);

        QJsonObject data;
        data["id"] = QString::fromStdString(object->GetID());
        data["name"] = name;
        data["sceneId"] = QString::fromStdString(scene->GetID());
        return McpResult::Ok(data);
    });

    dispatcher.RegisterTool("object.duplicate", [](const QJsonObject& params) {
        GameObject* source = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &source); !resolved.ok)
            return resolved;

        McpResult result = CallGameObjectBinding("duplicate", {source->GetID()});
        if (!result.ok) return result;

        const QString newId = result.data.toString();
        if (newId.isEmpty())
            return McpResult::Error(ObjectNotFound, "duplicate failed for id " +
                                                    QString::fromStdString(source->GetID()));
        QJsonObject data;
        data["id"] = newId;
        return McpResult::Ok(data);
    });

    // Type name must match Component::GetTypeName() -- it goes to SerializableFactory,
    // so an unregistered name yields nothing rather than an error.
    dispatcher.RegisterTool("object.add_component", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        const QString type = params.value("type").toString();
        if (type.isEmpty())
            return McpResult::Error(ObjectNotFound, "missing \"type\" (e.g. \"TextRenderer\")");

        // Distinguished up front, because the binding returns an empty string for both
        // reasons and reporting "unknown type Transform" would be actively misleading.
        const std::string typeName = type.toStdString();
        if (Component::IsSingleton(typeName) && object->HasComponentByName(typeName))
            return McpResult::Error(WrongMode,
                type + " is a single-instance component and this object already has one");

        McpResult result = CallGameObjectBinding("add_component", {object->GetID(), typeName});
        if (!result.ok) return result;

        const QString componentId = result.data.toString();
        if (componentId.isEmpty())
            return McpResult::Error(ObjectNotFound,
                "no component type named \"" + type + "\" is registered");

        QJsonObject data;
        data["componentId"] = componentId;
        data["type"] = type;
        return McpResult::Ok(data);
    });

    dispatcher.RegisterTool("object.destroy", [](const QJsonObject& params) {
        GameObject* target = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &target); !resolved.ok)
            return resolved;

        const std::string id = target->GetID();
        ImpactClarification impact = AnalyzeObjectDestruction(target);
        const QString clarificationId = params.value("clarificationId").toString();
        if (clarificationId.isEmpty()) {
            const QString requestId = UserClarificationService::Get()->Create(
                std::move(impact.request));
            return McpResult::Ok(QJsonObject{
                {"status", "clarification_required"},
                {"requestId", requestId},
                {"action", "destroy_object"},
                {"objectId", QString::fromStdString(id)},
                {"affected", impact.affected},
                {"affectedTotal", impact.affectedTotal},
                {"truncated", impact.truncated}
            });
        }

        QJsonObject clarification;
        QString authorizationError;
        if (!UserClarificationService::Get()->Consume(
                clarificationId, impact.request.scope, QStringLiteral("destroy"),
                &clarification, &authorizationError)) {
            return McpResult::Error(InvalidParams, authorizationError);
        }
        McpResult result = CallGameObjectBinding("shut_down", {id});
        if (!result.ok) return result;

        QJsonObject data;
        data["status"] = "destroyed";
        data["destroyed"] = QString::fromStdString(id);
        data["clarification"] = clarification;
        data["affected"] = impact.affected;
        return McpResult::Ok(data);
    });
}

} // namespace mcp
