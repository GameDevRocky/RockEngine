#include "mcp/PyApiCall.hpp"

#include "mcp/PyBind.hpp"
#include "mcp/ToolSupport.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace py = pybind11;

namespace mcp::pyapi {

namespace {

// Vector2 (Domain/lib/utils/re_math) is the one non-primitive that crosses this
// boundary often enough to be worth special-casing: reported as {"x":..,"y":..} and
// accepted as either that or a two-element array.
bool LooksLikeVector(const py::object& value) {
    return py::hasattr(value, "x") && py::hasattr(value, "y");
}

QJsonValue ToJson(const py::handle& value) {
    if (value.is_none()) return QJsonValue();
    if (py::isinstance<py::bool_>(value)) return value.cast<bool>();
    if (py::isinstance<py::int_>(value)) return value.cast<qint64>();
    if (py::isinstance<py::float_>(value)) return value.cast<double>();
    if (py::isinstance<py::str>(value)) return QString::fromStdString(value.cast<std::string>());

    if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value)) {
        QJsonArray array;
        for (const py::handle& item : value)
            array.append(ToJson(item));
        return array;
    }

    if (py::isinstance<py::dict>(value)) {
        QJsonObject object;
        for (const auto& [key, item] : value.cast<py::dict>())
            object[QString::fromStdString(py::str(key).cast<std::string>())] = ToJson(item);
        return object;
    }

    py::object owned = py::reinterpret_borrow<py::object>(value);
    if (LooksLikeVector(owned)) {
        QJsonObject vector;
        vector["x"] = owned.attr("x").cast<double>();
        vector["y"] = owned.attr("y").cast<double>();
        return vector;
    }

    // Anything else (a handler instance, an asset wrapper) is reported by its engine
    // id when it has one, since that is what a client can actually act on.
    if (py::hasattr(owned, "id"))
        return QString::fromStdString(py::str(owned.attr("id")).cast<std::string>());
    return QString::fromStdString(py::str(owned).cast<std::string>());
}

py::object ToPython(const QJsonValue& value) {
    switch (value.type()) {
        case QJsonValue::Bool:   return py::bool_(value.toBool());
        case QJsonValue::Double: return py::float_(value.toDouble());
        case QJsonValue::String: return py::str(value.toString().toStdString());
        case QJsonValue::Array: {
            py::list list;
            for (const QJsonValue& item : value.toArray())
                list.append(ToPython(item));
            return list;
        }
        case QJsonValue::Object: {
            const QJsonObject object = value.toObject();
            // An {x, y} pair becomes a tuple, which every Vector2 setter accepts.
            if (object.contains("x") && object.contains("y"))
                return py::make_tuple(object.value("x").toDouble(), object.value("y").toDouble());

            py::dict dict;
            for (auto it = object.begin(); it != object.end(); ++it)
                dict[py::str(it.key().toStdString())] = ToPython(it.value());
            return dict;
        }
        default: return py::none();
    }
}

// Import the handler module and construct the handler bound to this object.
py::object MakeHandler(const HandlerClass& handler, const std::string& objectId) {
    py::module_ module = py::module_::import(handler.module);
    return module.attr(handler.name)(objectId);
}

McpResult FromPythonError(const py::error_already_set& e) {
    return McpResult::Error(PythonError, QString::fromUtf8(e.what()));
}

// Every component type reachable from a type-name-driven tool. Handlers a GameObject
// can hold several of (colliders, joints, audio sources) resolve to the first instance
// of that type, which is what Component._resolve_component_id() falls back to.
constexpr HandlerClass kAllHandlers[] = {
    kTransform,
    kSpriteRenderer,
    kRigidBody,
    kTextRenderer,
    {"Domain.lib.api.components.camera_handler", "Camera", "Camera"},
    {"Domain.lib.api.components.animator_handler", "Animator", "Animator"},
    {"Domain.lib.api.components.particle_component_handler", "ParticleComponent", "ParticleComponent"},
    {"Domain.lib.api.components.audio_source_handler", "AudioSource", "AudioSource"},
    {"Domain.lib.api.components.audio_listener_handler", "AudioListener", "AudioListener"},
    {"Domain.lib.api.components.box_collider_handler", "BoxCollider", "BoxCollider"},
    {"Domain.lib.api.components.circle_collider_handler", "CircleCollider", "CircleCollider"},
    {"Domain.lib.api.components.capsule_collider_handler", "CapsuleCollider", "CapsuleCollider"},
};

} // namespace

const HandlerClass* HandlerForType(const QString& typeName) {
    for (const HandlerClass& handler : kAllHandlers) {
        if (typeName.compare(QString::fromUtf8(handler.typeName), Qt::CaseInsensitive) == 0)
            return &handler;
    }
    return nullptr;
}

McpResult GetProperty(const HandlerClass& handler, const std::string& objectId, const char* property) {
    if (!support::FindGameObject(objectId))
        return McpResult::Error(ObjectNotFound, "no GameObject with id " + QString::fromStdString(objectId));

    py::gil_scoped_acquire gil;
    try {
        return McpResult::Ok(ToJson(MakeHandler(handler, objectId).attr(property)));
    } catch (const py::error_already_set& e) {
        return FromPythonError(e);
    }
}

McpResult SetProperty(const HandlerClass& handler, const std::string& objectId, const char* property,
                      const QJsonValue& value) {
    if (!support::FindGameObject(objectId))
        return McpResult::Error(ObjectNotFound, "no GameObject with id " + QString::fromStdString(objectId));

    py::gil_scoped_acquire gil;
    try {
        py::object instance = MakeHandler(handler, objectId);
        instance.attr(property) = ToPython(value);
        // Read back rather than echoing the request: the engine may clamp or normalize,
        // and the client wants the value that actually landed.
        return McpResult::Ok(ToJson(instance.attr(property)));
    } catch (const py::error_already_set& e) {
        return FromPythonError(e);
    }
}

McpResult SetAssetProperty(const HandlerClass& handler, const std::string& objectId,
                           const char* property, const HandlerClass& assetClass,
                           const std::string& assetId) {
    if (!support::FindGameObject(objectId))
        return McpResult::Error(ObjectNotFound, "no GameObject with id " + QString::fromStdString(objectId));

    py::gil_scoped_acquire gil;
    try {
        py::object instance = MakeHandler(handler, objectId);
        if (assetId.empty()) {
            instance.attr(property) = py::none();
        } else {
            py::module_ assetModule = py::module_::import(assetClass.module);
            instance.attr(property) = assetModule.attr(assetClass.name)(assetId);
        }
        return McpResult::Ok(ToJson(instance.attr(property)));
    } catch (const py::error_already_set& e) {
        return FromPythonError(e);
    }
}

McpResult CallMethod(const HandlerClass& handler, const std::string& objectId, const char* method,
                     const std::vector<QJsonValue>& args) {
    if (!support::FindGameObject(objectId))
        return McpResult::Error(ObjectNotFound, "no GameObject with id " + QString::fromStdString(objectId));

    py::gil_scoped_acquire gil;
    try {
        py::tuple callArgs(args.size());
        for (std::size_t i = 0; i < args.size(); ++i)
            callArgs[i] = ToPython(args[i]);
        return McpResult::Ok(ToJson(MakeHandler(handler, objectId).attr(method)(*callArgs)));
    } catch (const py::error_already_set& e) {
        return FromPythonError(e);
    }
}

} // namespace mcp::pyapi
