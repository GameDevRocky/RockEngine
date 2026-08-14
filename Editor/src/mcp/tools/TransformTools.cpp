#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/PyApiCall.hpp"

#include <QJsonObject>

namespace mcp {

namespace {

// "<group>.get_<tool>" / "<group>.set_<tool>".
QString GetName(const QString& group, const QString& tool) { return group + ".get_" + tool; }
QString SetName(const QString& group, const QString& tool) { return group + ".set_" + tool; }

// A get/set pair over one handler property, which is the shape of nearly every
// component tool. `valueKey` is the params field the setter reads.
void RegisterPropertyPair(McpDispatcher& dispatcher, const QString& group, const QString& tool,
                          const pyapi::HandlerClass& handler, const char* property,
                          const char* valueKey) {
    dispatcher.RegisterTool(GetName(group, tool).toStdString(),
        [handler, property](const QJsonObject& params) {
            return pyapi::GetProperty(handler, params.value("id").toString().toStdString(), property);
        });

    dispatcher.RegisterTool(SetName(group, tool).toStdString(),
        [handler, property, valueKey](const QJsonObject& params) {
            if (!params.contains(valueKey))
                return McpResult::Error(ObjectNotFound, QString("missing \"%1\"").arg(valueKey));
            return pyapi::SetProperty(handler, params.value("id").toString().toStdString(), property,
                                      params.value(valueKey));
        });
}

// Vector properties accept either {"value": {"x":..,"y":..}} or a flat {"x":..,"y":..}.
void RegisterVectorPair(McpDispatcher& dispatcher, const QString& group, const QString& tool,
                        const pyapi::HandlerClass& handler, const char* property) {
    dispatcher.RegisterTool(GetName(group, tool).toStdString(),
        [handler, property](const QJsonObject& params) {
            return pyapi::GetProperty(handler, params.value("id").toString().toStdString(), property);
        });

    dispatcher.RegisterTool(SetName(group, tool).toStdString(),
        [handler, property](const QJsonObject& params) {
            QJsonValue value = params.value("value");
            if (value.isUndefined()) {
                if (!params.contains("x") || !params.contains("y"))
                    return McpResult::Error(ObjectNotFound, "missing \"x\"/\"y\" (or \"value\")");
                QJsonObject vector;
                vector["x"] = params.value("x");
                vector["y"] = params.value("y");
                value = vector;
            }
            return pyapi::SetProperty(handler, params.value("id").toString().toStdString(), property, value);
        });
}

} // namespace

void RegisterTransformTools(McpDispatcher& dispatcher) {
    RegisterVectorPair(dispatcher, "transform", "position", pyapi::kTransform, "position");
    RegisterVectorPair(dispatcher, "transform", "scale", pyapi::kTransform, "scale");
    RegisterVectorPair(dispatcher, "transform", "world_position", pyapi::kTransform, "world_position");
    RegisterVectorPair(dispatcher, "transform", "world_scale", pyapi::kTransform, "world_scale");

    RegisterPropertyPair(dispatcher, "transform", "rotation", pyapi::kTransform, "rotation", "value");
    RegisterPropertyPair(dispatcher, "transform", "world_rotation", pyapi::kTransform,
                         "world_rotation", "value");
}

void RegisterComponentTools(McpDispatcher& dispatcher) {
    // `enabled` lives on the base Component handler, so this works for any component
    // type -- the type name is what picks which of an object's components is meant.
    const auto resolveTyped = [](const QJsonObject& params,
                                 const pyapi::HandlerClass** out) -> McpResult {
        const QString type = params.value("type").toString();
        if (type.isEmpty())
            return McpResult::Error(ObjectNotFound, "missing \"type\" (e.g. \"SpriteRenderer\")");
        const pyapi::HandlerClass* handler = pyapi::HandlerForType(type);
        if (!handler)
            return McpResult::Error(ObjectNotFound, "no scripting handler for component type " + type);
        *out = handler;
        return McpResult::Ok();
    };

    dispatcher.RegisterTool("component.get_enabled", [resolveTyped](const QJsonObject& params) {
        const pyapi::HandlerClass* handler = nullptr;
        if (McpResult resolved = resolveTyped(params, &handler); !resolved.ok) return resolved;
        return pyapi::GetProperty(*handler, params.value("id").toString().toStdString(), "enabled");
    });

    dispatcher.RegisterTool("component.set_enabled", [resolveTyped](const QJsonObject& params) {
        const pyapi::HandlerClass* handler = nullptr;
        if (McpResult resolved = resolveTyped(params, &handler); !resolved.ok) return resolved;
        if (!params.contains("value"))
            return McpResult::Error(ObjectNotFound, "missing \"value\"");
        return pyapi::SetProperty(*handler, params.value("id").toString().toStdString(), "enabled",
                                  params.value("value"));
    });

    // Property names here mirror the Python handlers exactly (flipX/flipY are camelCase
    // there), so a tool name maps to something greppable in Domain/lib/api.
    RegisterPropertyPair(dispatcher, "sprite_renderer", "flip_x", pyapi::kSpriteRenderer, "flipX", "value");
    RegisterPropertyPair(dispatcher, "sprite_renderer", "flip_y", pyapi::kSpriteRenderer, "flipY", "value");
    RegisterPropertyPair(dispatcher, "sprite_renderer", "color", pyapi::kSpriteRenderer, "color", "value");
    RegisterPropertyPair(dispatcher, "sprite_renderer", "visible", pyapi::kSpriteRenderer, "visible", "value");
    RegisterVectorPair(dispatcher, "sprite_renderer", "uv_offset", pyapi::kSpriteRenderer, "uv_offset");
    RegisterVectorPair(dispatcher, "sprite_renderer", "uv_scale", pyapi::kSpriteRenderer, "uv_scale");

    RegisterPropertyPair(dispatcher, "text_renderer", "text", pyapi::kTextRenderer, "text", "value");
    RegisterPropertyPair(dispatcher, "text_renderer", "color", pyapi::kTextRenderer, "color", "value");
    RegisterPropertyPair(dispatcher, "text_renderer", "font_size", pyapi::kTextRenderer,
                         "font_size", "value");
    RegisterPropertyPair(dispatcher, "text_renderer", "font", pyapi::kTextRenderer, "font", "value");
    RegisterPropertyPair(dispatcher, "text_renderer", "visible", pyapi::kTextRenderer,
                         "visible", "value");
    // Write-only on the Python side (the getter raises), so no get_ counterpart.
    dispatcher.RegisterTool("text_renderer.set_max_width", [](const QJsonObject& params) {
        return pyapi::SetProperty(pyapi::kTextRenderer, params.value("id").toString().toStdString(),
                                  "max_width", params.value("value"));
    });

    RegisterVectorPair(dispatcher, "rigidbody", "velocity", pyapi::kRigidBody, "velocity");
    RegisterPropertyPair(dispatcher, "rigidbody", "use_gravity", pyapi::kRigidBody, "use_gravity", "value");
    RegisterPropertyPair(dispatcher, "rigidbody", "body_type", pyapi::kRigidBody, "body_type", "value");
    RegisterPropertyPair(dispatcher, "rigidbody", "lock_rotation", pyapi::kRigidBody,
                         "lock_rotation", "value");

    // These handler methods take a single Vector2, not two floats.
    const auto vectorArg = [](const QJsonObject& params) {
        QJsonObject vector;
        vector["x"] = params.value("x");
        vector["y"] = params.value("y");
        return QJsonValue(vector);
    };

    dispatcher.RegisterTool("rigidbody.apply_force", [vectorArg](const QJsonObject& params) {
        return pyapi::CallMethod(pyapi::kRigidBody, params.value("id").toString().toStdString(),
                                 "apply_force", {vectorArg(params)});
    });
    dispatcher.RegisterTool("rigidbody.apply_impulse", [vectorArg](const QJsonObject& params) {
        return pyapi::CallMethod(pyapi::kRigidBody, params.value("id").toString().toStdString(),
                                 "apply_impulse", {vectorArg(params)});
    });
    dispatcher.RegisterTool("rigidbody.apply_torque", [](const QJsonObject& params) {
        return pyapi::CallMethod(pyapi::kRigidBody, params.value("id").toString().toStdString(),
                                 "apply_torque", {params.value("value")});
    });
}

} // namespace mcp
