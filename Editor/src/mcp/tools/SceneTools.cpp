#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"

#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace mcp {

namespace {

QJsonObject DescribeTransform(GameObject* object) {
    QJsonObject result;
    if (Transform* transform = object->GetTransform()) {
        QJsonObject position;
        position["x"] = transform->localPosition.x;
        position["y"] = transform->localPosition.y;
        result["position"] = position;
        result["rotation"] = transform->localRotation;

        QJsonObject scale;
        scale["x"] = transform->localScale.x;
        scale["y"] = transform->localScale.y;
        result["scale"] = scale;
    }
    return result;
}

// Read-only hierarchy walks go straight to Scene/Registry rather than through
// Domain/lib/api: there is no scripting-side equivalent to wrap (scene_system.py only
// exposes save), so there is no id-resolution or coercion logic being skipped.
//
// A tilemap parent can hold hundreds of children, so `maxDepth` exists to keep a
// listing readable. It counts LEVELS RETURNED: 1 is roots only, 2 is roots plus their
// children. 0 means unlimited -- the default, because silently hiding part of a
// hierarchy is worse than a long response that was asked for. A node whose children
// were cut still reports childCount, so the caller knows to look deeper.
QJsonObject DescribeNode(GameObject* object, int depth, int maxDepth) {
    QJsonObject node;
    node["id"] = QString::fromStdString(object->GetID());
    node["name"] = QString::fromStdString(object->GetName());
    node["active"] = object->GetActive();

    Transform* transform = object->GetTransform();
    if (!transform) return node;

    // Copied, not held by reference: GetChildren() hands back a generation-cached
    // vector that its own header warns against holding across a structural change,
    // and this recurses arbitrarily deep before coming back to it.
    const std::vector<Transform*> childTransforms = transform->GetChildren();
    if (childTransforms.empty()) return node;

    node["childCount"] = static_cast<int>(childTransforms.size());

    // This node sits at level depth+1; its children would be level depth+2.
    if (maxDepth > 0 && depth + 2 > maxDepth) {
        node["childrenOmitted"] = true;
        return node;
    }

    QJsonArray children;
    for (Transform* child : childTransforms) {
        if (GameObject* childObject = child->GetGameObject())
            children.append(DescribeNode(childObject, depth + 1, maxDepth));
    }
    if (!children.isEmpty())
        node["children"] = children;
    return node;
}

SceneManager* ActiveSceneManager() {
    Container* container = support::ActiveContainer();
    return container ? container->FindSystem<SceneManager>() : nullptr;
}

std::vector<Scene*> LoadedScenes() {
    SceneManager* sceneManager = ActiveSceneManager();
    return sceneManager ? sceneManager->GetScenes() : std::vector<Scene*>{};
}

} // namespace

void RegisterSceneTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("scene.list", [](const QJsonObject&) {
        QJsonArray scenes;
        for (Scene* scene : LoadedScenes()) {
            QJsonObject entry;
            entry["id"] = QString::fromStdString(scene->GetID());
            entry["name"] = QString::fromStdString(scene->GetName());
            entry["path"] = QString::fromStdString(scene->GetPath());
            scenes.append(entry);
        }
        QJsonObject result;
        result["scenes"] = scenes;
        return McpResult::Ok(result);
    });

    // The editor opens with nothing loaded (scenes are normally dragged onto the
    // hierarchy), so without this an external caller has no way to get a world at all.
    dispatcher.RegisterTool("scene.load", [](const QJsonObject& params) {
        const QString path = params.value("path").toString();
        if (path.isEmpty())
            return McpResult::Error(ObjectNotFound,
                                    "missing \"path\" (e.g. \"Domain/sandbox/default.scene\")");

        SceneManager* sceneManager = ActiveSceneManager();
        if (!sceneManager)
            return McpResult::Error(WrongMode, "no SceneManager on the active container");

        // Relative paths resolve against the asset root, so a caller can use the same
        // repo-relative path that appears in project.build.
        const std::string resolved = QDir::isAbsolutePath(path)
            ? path.toStdString()
            : EngineUtils::GetAssetPath(path.toStdString());

        if (!QFileInfo::exists(QString::fromStdString(resolved)))
            return McpResult::Error(ObjectNotFound,
                                    "no scene file at " + QString::fromStdString(resolved));

        // Asynchronous: the YAML parse runs on a worker and the object graph is built
        // from a later frame's main step, so the scene is not queryable on return.
        sceneManager->LoadScene(resolved);

        QJsonObject data;
        data["status"] = "load_requested";
        data["path"] = QString::fromStdString(resolved);
        data["poll"] = "scene.list";
        return McpResult::Ok(data);
    });

    // Mirrors FolderViewGui::CreateNewScene() (the Folder view's right-click New > Scene):
    // a minimal valid scene file -- name + id + empty gameobjects/components -- written to
    // disk first, exactly like Unity's Assets > Create > Scene, then loaded the same way a
    // human dragging that file onto the hierarchy would, so the caller gets back a real,
    // immediately-queryable Scene rather than just a file on disk.
    dispatcher.RegisterTool("scene.new", [](const QJsonObject& params) {
        SceneManager* sceneManager = ActiveSceneManager();
        if (!sceneManager)
            return McpResult::Error(WrongMode, "no SceneManager on the active container");

        QString name = params.value("name").toString();
        if (name.isEmpty()) name = "New Scene";

        // Every checked-in scene lives directly under Domain/sandbox (there is no
        // dedicated "Scenes" subfolder), so that is the default placement.
        QString directory = params.value("directory").toString();
        if (directory.isEmpty()) directory = "Domain/sandbox";
        const QString resolvedDir = QDir::isAbsolutePath(directory)
            ? directory
            : QString::fromStdString(EngineUtils::GetAssetPath(directory.toStdString()));

        QDir dir(resolvedDir);
        if (!dir.exists())
            return McpResult::Error(ObjectNotFound, "no such directory: " + resolvedDir);

        // Same collision-avoidance as the Folder view's "New" menu: "<name> 1.scene",
        // "<name> 2.scene", ... rather than overwriting an existing file.
        QString candidate = name + ".scene";
        for (int n = 1; QFileInfo::exists(dir.filePath(candidate)); ++n)
            candidate = QString("%1 %2.scene").arg(name).arg(n);
        const QString path = dir.filePath(candidate);
        const QString sceneName = QFileInfo(path).completeBaseName();

        YAML::Node node;
        node["name"] = sceneName.toStdString();
        node["id"] = EngineUtils::GenerateUUID();
        node["gameobjects"] = YAML::Node(YAML::NodeType::Sequence);
        node["components"]  = YAML::Node(YAML::NodeType::Sequence);

        std::ofstream out(path.toStdString());
        if (!out.is_open())
            return McpResult::Error(WrongMode, "failed to create scene file at " + path);
        out << node;
        out.close();

        // Asynchronous, same as scene.load: parsed on a worker, installed from a later
        // frame's main step, so the scene is not queryable until scene.list reports it.
        sceneManager->LoadScene(path.toStdString());

        QJsonObject data;
        data["status"] = "created";
        data["name"] = sceneName;
        data["path"] = path;
        data["poll"] = "scene.list";
        return McpResult::Ok(data);
    });

    // Drops a loaded scene from the live world without touching its file -- the
    // in-memory counterpart to scene.new, for backing out a scene that was loaded (or
    // created) by mistake. RemoveScene tears the object graph down on the next job
    // pump rather than synchronously (see its header comment), so this is
    // asynchronous exactly like scene.load: poll scene.list to see it gone.
    dispatcher.RegisterTool("scene.unload", [](const QJsonObject& params) {
        SceneManager* sceneManager = ActiveSceneManager();
        if (!sceneManager)
            return McpResult::Error(WrongMode, "no SceneManager on the active container");

        const std::vector<Scene*> scenes = LoadedScenes();
        QString sceneId = params.value("sceneId").toString();
        if (sceneId.isEmpty()) {
            if (scenes.size() != 1)
                return McpResult::Error(ObjectNotFound,
                    "pass \"sceneId\" -- there is not exactly one loaded scene to default to");
            sceneId = QString::fromStdString(scenes.front()->GetID());
        } else {
            const bool loaded = std::any_of(scenes.begin(), scenes.end(), [&](Scene* scene) {
                return QString::fromStdString(scene->GetID()) == sceneId;
            });
            if (!loaded)
                return McpResult::Error(ObjectNotFound, "no loaded scene with id " + sceneId);
        }

        sceneManager->RemoveScene(sceneId.toStdString());

        QJsonObject data;
        data["status"] = "unload_requested";
        data["sceneId"] = sceneId;
        data["poll"] = "scene.list";
        return McpResult::Ok(data);
    });

    dispatcher.RegisterTool("scene.save", [](const QJsonObject& params) {
        SceneManager* sceneManager = ActiveSceneManager();
        if (!sceneManager)
            return McpResult::Error(WrongMode, "no SceneManager on the active container");

        QString sceneId = params.value("sceneId").toString();
        if (sceneId.isEmpty()) {
            const std::vector<Scene*> scenes = LoadedScenes();
            if (scenes.size() != 1)
                return McpResult::Error(ObjectNotFound,
                    "pass \"sceneId\" -- there is not exactly one loaded scene to default to");
            sceneId = QString::fromStdString(scenes.front()->GetID());
        }

        // allowRuntimeSave stays false: writing play-mode state over an authored scene
        // needs to be a deliberate act, not a side effect of an agent poking around.
        if (!sceneManager->SaveScene(sceneId.toStdString()))
            return McpResult::Error(WrongMode,
                "save refused (unknown scene, no path, play mode, or I/O failure)");

        QJsonObject data;
        data["saved"] = sceneId;
        return McpResult::Ok(data);
    });

    // Whole loaded-world tree by default; one scene when given a sceneId.
    dispatcher.RegisterTool("scene.hierarchy", [](const QJsonObject& params) {
        const QString wanted = params.value("sceneId").toString();
        const int maxDepth = std::max(0, params.value("maxDepth").toInt(0));

        QJsonArray scenes;
        for (Scene* scene : LoadedScenes()) {
            if (!wanted.isEmpty() && QString::fromStdString(scene->GetID()) != wanted)
                continue;

            QJsonArray roots;
            for (GameObject* root : scene->GetRootObjects())
                roots.append(DescribeNode(root, 0, maxDepth));

            QJsonObject entry;
            entry["id"] = QString::fromStdString(scene->GetID());
            entry["name"] = QString::fromStdString(scene->GetName());
            entry["roots"] = roots;
            scenes.append(entry);
        }

        if (scenes.isEmpty() && !wanted.isEmpty())
            return McpResult::Error(ObjectNotFound, "no loaded scene with id " + wanted);

        QJsonObject result;
        result["scenes"] = scenes;
        return McpResult::Ok(result);
    });

    dispatcher.RegisterTool("object.get_details", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        QJsonArray components;
        for (Component* component : object->GetAllComponents()) {
            QJsonObject entry;
            entry["id"] = QString::fromStdString(component->GetID());
            entry["type"] = QString::fromStdString(component->GetTypeName());
            entry["enabled"] = component->GetEnabled();
            components.append(entry);
        }

        QJsonObject result;
        result["id"] = QString::fromStdString(object->GetID());
        result["name"] = QString::fromStdString(object->GetName());
        result["tag"] = QString::fromStdString(object->GetTag());
        result["active"] = object->GetActive();
        result["components"] = components;
        result["transform"] = DescribeTransform(object);
        if (Scene* scene = object->GetScene())
            result["sceneId"] = QString::fromStdString(scene->GetID());
        return McpResult::Ok(result);
    });

    // Drives the editor's real selection, so the Hierarchy and Inspector panels follow
    // along -- the human watching sees whatever is being inspected or edited.
    dispatcher.RegisterTool("object.select", [](const QJsonObject& params) {
        Container* container = support::ActiveContainer();
        SelectionManager* selection = container ? container->FindSystem<SelectionManager>() : nullptr;
        if (!selection)
            return McpResult::Error(WrongMode, "no SelectionManager on the active container");

        // No id clears the selection.
        const QString id = params.value("id").toString();
        if (id.isEmpty()) {
            selection->ClearSelection();
            QJsonObject result;
            result["selected"] = QJsonValue();
            return McpResult::Ok(result);
        }

        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        selection->Select(object->GetID());
        QJsonObject result;
        result["selected"] = QString::fromStdString(object->GetID());
        result["name"] = QString::fromStdString(object->GetName());
        return McpResult::Ok(result);
    });
}

} // namespace mcp
