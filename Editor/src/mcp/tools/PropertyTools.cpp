#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/YamlJson.hpp"

#include "engine/audio/AudioClip.hpp"
#include "engine/commands/MacroCommand.hpp"
#include "engine/components/Animator.hpp"
#include "engine/components/ComponentActions.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/components/AudioSource.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/DistanceJoint.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/MotorJoint.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/TextRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/WeldJoint.hpp"
#include "engine/components/WheelJoint.hpp"
#include "engine/core/Command.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/LayerManager.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace mcp {
namespace {

QString DisplayName(QString key) {
    key.replace('.', ' ');
    key.replace('_', ' ');
    bool upper = true;
    for (int i = 0; i < key.size(); ++i) {
        QChar c = key[i];
        if (upper && c.isLetter()) key[i] = c.toUpper();
        upper = c.isSpace();
    }
    return key;
}

QJsonValue ToJson(const glm::vec2& v) { return QJsonArray{v.x, v.y}; }
QJsonValue ToJson(const glm::vec3& v) { return QJsonArray{v.x, v.y, v.z}; }
QJsonValue ToJson(const glm::vec4& v) { return QJsonArray{v.x, v.y, v.z, v.w}; }
QJsonValue ToJson(const std::string& v) { return QString::fromStdString(v); }
QJsonValue ToJson(bool v) { return v; }
QJsonValue ToJson(int v) { return v; }
QJsonValue ToJson(float v) { return static_cast<double>(v); }

// True for the std::vector alternatives of ScriptFieldValue (list[T] script
// fields), so the visits below dispatch on "is a list" rather than naming every
// element type -- there are seven and the set grows with the scripting API.
template <typename T> constexpr bool kIsVector = false;
template <typename T> constexpr bool kIsVector<std::vector<T>> = true;

template <typename T>
QJsonValue VectorToJson(const std::vector<T>& values) {
    QJsonArray result;
    for (const auto& value : values) {
        if constexpr (std::is_same_v<T, bool>) result.append(static_cast<bool>(value));
        else result.append(ToJson(value));
    }
    return result;
}

QString ParseBool(const QJsonValue& value, bool& out) {
    if (!value.isBool()) return "expected a boolean";
    out = value.toBool();
    return {};
}

QString ParseInt(const QJsonValue& value, int& out) {
    if (!value.isDouble()) return "expected an integer";
    const double n = value.toDouble();
    if (!std::isfinite(n) || std::floor(n) != n) return "expected an integer";
    out = static_cast<int>(n);
    return {};
}

QString ParseFloat(const QJsonValue& value, float& out) {
    if (!value.isDouble() || !std::isfinite(value.toDouble())) return "expected a finite number";
    out = static_cast<float>(value.toDouble());
    return {};
}

QString ParseString(const QJsonValue& value, std::string& out) {
    if (!value.isString()) return "expected a string";
    out = value.toString().toStdString();
    return {};
}

QString ValidateReference(const QString& type, const std::string& id) {
    if (type.isEmpty() || id.empty()) return {};
    const QString normalized = type.toLower();
    if (normalized.startsWith("gameobject"))
        return support::FindGameObject(id) ? QString{} : "no GameObject with id " + QString::fromStdString(id);
    AssetManager& assets = AssetManager::Get();
    bool exists = false;
    if (normalized == "sprite") exists = assets.GetSprite(id);
    else if (normalized == "material") exists = assets.GetMaterial(id);
    else if (normalized == "texture2d" || normalized == "texture") exists = assets.GetTexture(id);
    else if (normalized == "font") exists = assets.GetFont(id);
    else if (normalized == "audioclip" || normalized == "audio") exists = assets.GetAudioClip(id);
    else if (normalized == "shader") exists = assets.GetShader(id);
    else return {};
    return exists ? QString{} : "no " + type + " asset with id " + QString::fromStdString(id);
}

template <int N, typename Vec>
QString ParseVec(const QJsonValue& value, Vec& out) {
    if (!value.isArray() || value.toArray().size() != N)
        return QString("expected an array of %1 numbers").arg(N);
    const QJsonArray a = value.toArray();
    for (int i = 0; i < N; ++i) {
        if (!a[i].isDouble() || !std::isfinite(a[i].toDouble()))
            return QString("element %1 must be a finite number").arg(i);
        out[i] = static_cast<float>(a[i].toDouble());
    }
    return {};
}

struct Property {
    QString name;
    QString type;
    QString referenceType;
    QJsonArray choices;
    bool readOnly = false;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    QString description;
    std::function<QJsonValue()> get;
    std::function<QString(const QJsonValue&)> set;

    QJsonObject Describe(bool includeValue) const {
        QJsonObject result;
        result["name"] = name;
        result["displayName"] = DisplayName(name);
        result["type"] = type;
        result["writable"] = !readOnly && static_cast<bool>(set);
        if (!referenceType.isEmpty()) result["referenceType"] = referenceType;
        if (!choices.isEmpty()) result["choices"] = choices;
        if (minimum) result["minimum"] = *minimum;
        if (maximum) result["maximum"] = *maximum;
        if (step) result["step"] = *step;
        if (!description.isEmpty()) result["description"] = description;
        if (includeValue && get) result["value"] = get();
        return result;
    }
};

class PropertyBag {
public:
    void Bool(const QString& name, std::function<bool()> get, std::function<void(bool)> set = {}) {
        Property p{name, "bool"};
        p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set](const QJsonValue& v) { bool n; QString e = ParseBool(v, n); if (e.isEmpty()) set(n); return e; };
        else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void Int(const QString& name, std::function<int()> get, std::function<void(int)> set = {},
             std::optional<double> min = {}, std::optional<double> max = {}, std::optional<double> step = {}) {
        Property p{name, "int"}; p.minimum = min; p.maximum = max; p.step = step;
        p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set, min, max](const QJsonValue& v) { int n; QString e = ParseInt(v, n); if (!e.isEmpty()) return e; if (min && n < *min) return QString("must be at least %1").arg(*min); if (max && n > *max) return QString("must be at most %1").arg(*max); set(n); return QString{}; };
        else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void Float(const QString& name, std::function<float()> get, std::function<void(float)> set = {},
               std::optional<double> min = {}, std::optional<double> max = {}, std::optional<double> step = {}) {
        Property p{name, "float"}; p.minimum = min; p.maximum = max; p.step = step;
        p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set, min, max](const QJsonValue& v) { float n; QString e = ParseFloat(v, n); if (!e.isEmpty()) return e; if (min && n < *min) return QString("must be at least %1").arg(*min); if (max && n > *max) return QString("must be at most %1").arg(*max); set(n); return QString{}; };
        else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void String(const QString& name, std::function<std::string()> get,
                std::function<void(const std::string&)> set = {}, const QString& referenceType = {}) {
        Property p{name, "string"}; p.referenceType = referenceType;
        p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set, referenceType](const QJsonValue& v) { std::string n; QString e = ParseString(v, n); if (e.isEmpty()) e = ValidateReference(referenceType, n); if (e.isEmpty()) set(n); return e; };
        else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void StringChoice(const QString& name, std::function<std::string()> get,
                      std::function<void(const std::string&)> set,
                      const std::vector<std::string>& names) {
        Property p{name, "enum"};
        QStringList choices;
        for (const std::string& n : names) { const QString q=QString::fromStdString(n); choices.push_back(q); p.choices.append(q); }
        p.get=[get]{return ToJson(get());};
        p.set=[set,choices](const QJsonValue& v){if(!v.isString())return QString("expected an enum name");for(const QString& choice:choices)if(choice.compare(v.toString(),Qt::CaseInsensitive)==0){set(choice.toStdString());return QString{};}return QString("unknown enum value; expected one of: %1").arg(choices.join(", "));};
        items.push_back(std::move(p));
    }

    void Vec2(const QString& name, std::function<glm::vec2()> get, std::function<void(glm::vec2)> set = {}) {
        Property p{name, "vec2"}; p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set](const QJsonValue& v) { glm::vec2 n; QString e = ParseVec<2>(v, n); if (e.isEmpty()) set(n); return e; }; else p.readOnly = true;
        items.push_back(std::move(p));
    }
    void Vec3(const QString& name, std::function<glm::vec3()> get, std::function<void(glm::vec3)> set = {}) {
        Property p{name, "vec3"}; p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set](const QJsonValue& v) { glm::vec3 n; QString e = ParseVec<3>(v, n); if (e.isEmpty()) set(n); return e; }; else p.readOnly = true;
        items.push_back(std::move(p));
    }
    void Vec4(const QString& name, std::function<glm::vec4()> get, std::function<void(glm::vec4)> set = {}) {
        Property p{name, "vec4"}; p.get = [get] { return ToJson(get()); };
        if (set) p.set = [set](const QJsonValue& v) { glm::vec4 n; QString e = ParseVec<4>(v, n); if (e.isEmpty()) set(n); return e; }; else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void StringList(const QString& name, std::function<std::vector<std::string>()> get,
                    std::function<void(std::vector<std::string>)> set = {}) {
        Property p{name, "string[]"}; p.get = [get] { return VectorToJson(get()); };
        if (set) p.set = [set](const QJsonValue& v) { if (!v.isArray()) return QString("expected an array of strings"); std::vector<std::string> result; for (const auto& x : v.toArray()) { if (!x.isString()) return QString("expected an array of strings"); result.push_back(x.toString().toStdString()); } set(std::move(result)); return QString{}; }; else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void Enum(const QString& name, std::function<int()> get, std::function<void(int)> set,
              std::initializer_list<const char*> names) {
        Property p{name, "enum"};
        QStringList choices;
        for (const char* n : names) { choices.push_back(n); p.choices.append(n); }
        p.get = [get, choices] { const int i = get(); return i >= 0 && i < choices.size() ? QJsonValue(choices[i]) : QJsonValue(i); };
        if (set) p.set = [set, choices](const QJsonValue& v) {
            int index = -1;
            if (v.isDouble()) { QString e = ParseInt(v, index); if (!e.isEmpty()) return e; }
            else if (v.isString()) { for (int i = 0; i < choices.size(); ++i) if (choices[i].compare(v.toString(), Qt::CaseInsensitive) == 0) { index = i; break; } }
            else return QString("expected an enum name or integer index");
            if (index < 0 || index >= choices.size()) return QString("unknown enum value; expected one of: %1").arg(choices.join(", "));
            set(index); return QString{};
        };
        else p.readOnly = true;
        items.push_back(std::move(p));
    }

    void ReadOnly(const QString& name, const QString& type, std::function<QJsonValue()> get) {
        Property p{name, type}; p.readOnly = true; p.get = std::move(get); items.push_back(std::move(p));
    }

    Property* Find(const QString& name) {
        auto it = std::find_if(items.begin(), items.end(), [&name](const Property& p) { return p.name == name; });
        return it == items.end() ? nullptr : &*it;
    }

    QJsonArray Describe(bool includeValues = true) const {
        QJsonArray result; for (const Property& p : items) result.append(p.Describe(includeValues)); return result;
    }

    void Add(Property property) { items.push_back(std::move(property)); }

private:
    std::vector<Property> items;
};

std::string ResourceId(Resource* resource) { return resource ? resource->GetID() : std::string{}; }

void AddJointProperties(PropertyBag& b, Joint* j) {
    b.String("connected_body", [j]{ return j->GetConnectedBody(); }, [j](const std::string& v){ j->SetConnectedBody(v); }, "GameObject");
    b.Bool("collide_connected", [j]{ return j->GetCollideConnected(); }, [j](bool v){ j->SetCollideConnected(v); });
    b.Vec2("local_anchor_a", [j]{ return j->GetLocalAnchorA(); }, [j](glm::vec2 v){ j->SetLocalAnchorA(v); });
    b.Vec2("local_anchor_b", [j]{ return j->GetLocalAnchorB(); }, [j](glm::vec2 v){ j->SetLocalAnchorB(v); });
    b.Bool("is_built", [j]{ return j->IsBuilt(); });
}

void AddColliderProperties(PropertyBag& b, Collider* c) {
    b.Vec2("center", [c]{ return c->GetCenter(); }, [c](glm::vec2 v){ c->SetCenter(v); });
    b.Float("density", [c]{ return c->GetDensity(); }, [c](float v){ c->SetDensity(v); }, 0.0);
    b.Float("bounciness", [c]{ return c->GetBounciness(); }, [c](float v){ c->SetBounciness(v); }, 0.0, 1.0);
    b.Bool("is_sensor", [c]{ return c->GetIsSensor(); }, [c](bool v){ c->SetIsSensor(v); });
    b.Float("friction", [c]{ return c->GetFriction(); }, [c](float v){ c->SetFriction(v); }, 0.0);
    b.Float("rolling_resistance", [c]{ return c->GetRollingResistance(); }, [c](float v){ c->SetRollingResistance(v); }, 0.0);
}

PropertyBag BuildComponentBag(Component* component) {
    PropertyBag b;
    std::vector<std::string> sortingLayers;
    if (Container* container=support::ActiveContainer()) if (LayerManager* layers=container->FindSystem<LayerManager>()) sortingLayers=layers->GetLayerNames();
    b.Bool("enabled", [component]{ return component->GetEnabled(); }, [component](bool v){ component->SetEnabled(v); });

    if (auto* t = dynamic_cast<Transform*>(component)) {
        b.Vec2("position", [t]{ return t->localPosition; }, [t](glm::vec2 v){ t->SetPosition(v); });
        b.Float("rotation", [t]{ return t->localRotation; }, [t](float v){ t->SetRotation(v); });
        b.Vec2("scale", [t]{ return t->localScale; }, [t](glm::vec2 v){ t->SetScale(v); });
        b.Vec2("world_position", [t]{ return t->GetWorldPosition(); }, [t](glm::vec2 v){ t->SetWorldPosition(v); });
        b.Float("world_rotation", [t]{ return t->GetWorldRotation(); }, [t](float v){ t->SetWorldRotation(v); });
        b.Vec2("world_scale", [t]{ return t->GetWorldScale(); }, [t](glm::vec2 v){ t->SetWorldScale(v); });
        b.String("parent", [t]{ Transform* p = t->GetParent(); return p && p->GetGameObject() ? p->GetGameObject()->GetID() : std::string{}; }, {}, "GameObject");
    } else if (auto* r = dynamic_cast<SpriteRenderer*>(component)) {
        b.String("sprite", [r]{ return r->GetSpriteID(); }, [r](const std::string& v){ std::string id=v; r->SetSprite(id); }, "Sprite");
        b.String("material", [r]{ return ResourceId(r->GetMaterial()); }, [r](const std::string& v){ std::string id=v; r->SetMaterial(id); }, "Material");
        b.Vec4("color", [r]{ return r->GetColor(); }, [r](glm::vec4 v){ r->SetColor(v); });
        b.Bool("visible", [r]{ return r->GetVisible(); }, [r](bool v){ bool n=v; r->SetVisible(n); });
        b.Bool("flip_x", [r]{ return r->GetFlipX(); }, [r](bool v){ r->SetFlipX(v); });
        b.Bool("flip_y", [r]{ return r->GetFlipY(); }, [r](bool v){ r->SetFlipY(v); });
        b.Vec2("uv_scale", [r]{ return r->GetUVScale(); }, [r](glm::vec2 v){ r->SetUVScale(v); });
        b.Vec2("uv_offset", [r]{ return r->GetUVOffset(); }, [r](glm::vec2 v){ r->SetUVOffset(v); });
        if(sortingLayers.empty())b.String("sorting_layer", [r]{ return r->GetSortingLayer(); }, [r](const std::string& v){ r->SetSortingLayer(v); });else b.StringChoice("sorting_layer",[r]{return r->GetSortingLayer();},[r](const std::string& v){r->SetSortingLayer(v);},sortingLayers);
        b.Int("sorting_order", [r]{ return r->GetSortingOrder(); }, [r](int v){ r->SetSortingOrder(v); });
    } else if (auto* r = dynamic_cast<TextRenderer*>(component)) {
        b.String("text", [r]{ return r->GetText(); }, [r](const std::string& v){ r->SetText(v); });
        b.String("font", [r]{ return r->GetFontID(); }, [r](const std::string& v){ r->SetFont(v); }, "Font");
        b.String("material", [r]{ return r->GetMaterialID(); }, [r](const std::string& v){ r->SetMaterial(v); }, "Material");
        b.Float("font_size", [r]{ return r->GetFontSize(); }, [r](float v){ r->SetFontSize(v); }, 0.01);
        b.Float("line_spacing", [r]{ return r->GetLineSpacing(); }, [r](float v){ r->SetLineSpacing(v); }, 0.0);
        b.Float("letter_spacing", [r]{ return r->GetLetterSpacing(); }, [r](float v){ r->SetLetterSpacing(v); });
        b.Float("max_width", [r]{ return r->GetMaxWidth(); }, [r](float v){ r->SetMaxWidth(v); }, 0.0);
        b.Enum("horizontal_align", [r]{ return static_cast<int>(r->GetHAlign()); }, [r](int v){ r->SetHAlign(static_cast<TextHAlign>(v)); }, {"Left","Center","Right"});
        b.Enum("vertical_align", [r]{ return static_cast<int>(r->GetVAlign()); }, [r](int v){ r->SetVAlign(static_cast<TextVAlign>(v)); }, {"Top","Middle","Baseline","Bottom"});
        b.Vec4("color", [r]{ return r->GetColor(); }, [r](glm::vec4 v){ r->SetColor(v); });
        b.Float("weight", [r]{ return r->GetWeight(); }, [r](float v){ r->SetWeight(v); });
        b.Vec4("outline_color", [r]{ return r->GetOutlineColor(); }, [r](glm::vec4 v){ r->SetOutlineColor(v); });
        b.Float("outline_width", [r]{ return r->GetOutlineWidth(); }, [r](float v){ r->SetOutlineWidth(v); }, 0.0);
        b.Bool("visible", [r]{ return r->GetVisible(); }, [r](bool v){ r->SetVisible(v); });
        if(sortingLayers.empty())b.String("sorting_layer", [r]{ return r->GetSortingLayer(); }, [r](const std::string& v){ r->SetSortingLayer(v); });else b.StringChoice("sorting_layer",[r]{return r->GetSortingLayer();},[r](const std::string& v){r->SetSortingLayer(v);},sortingLayers);
        b.Int("sorting_order", [r]{ return r->GetSortingOrder(); }, [r](int v){ r->SetSortingOrder(v); });
    } else if (auto* rb = dynamic_cast<RigidBody*>(component)) {
        b.Enum("body_type", [rb]{ const b2BodyType v=rb->GetBodyType(); return v==b2_staticBody?0:v==b2_kinematicBody?1:2; }, [rb](int v){ rb->SetBodyType(v==0?b2_staticBody:v==1?b2_kinematicBody:b2_dynamicBody); }, {"Static","Kinematic","Dynamic"});
        b.Bool("use_gravity", [rb]{ return rb->GetUseGravity(); }, [rb](bool v){ rb->SetUseGravity(v); });
        b.Bool("lock_rotation", [rb]{ return rb->GetLockRotation(); }, [rb](bool v){ rb->SetLockRotation(v); });
        b.Vec2("linear_velocity", [rb]{ return rb->GetLinearVelocity(); }, [rb](glm::vec2 v){ rb->SetLinearVelocity(v); });
        b.Float("angular_velocity", [rb]{ return rb->GetAngularVelocity(); }, [rb](float v){ rb->SetAngularVelocity(v); });
    } else if (auto* c = dynamic_cast<BoxCollider*>(component)) {
        AddColliderProperties(b, c); b.Vec2("size", [c]{ return c->GetSize(); }, [c](glm::vec2 v){ c->SetSize(v); });
    } else if (auto* c = dynamic_cast<CircleCollider*>(component)) {
        AddColliderProperties(b, c); b.Float("radius", [c]{ return c->GetRadius(); }, [c](float v){ c->SetRadius(v); }, 0.0);
    } else if (auto* c = dynamic_cast<CapsuleCollider*>(component)) {
        AddColliderProperties(b, c); b.Float("height", [c]{ return c->GetHeight(); }, [c](float v){ c->SetHeight(v); }, 0.0); b.Float("radius", [c]{ return c->GetRadius(); }, [c](float v){ c->SetRadius(v); }, 0.0);
    } else if (auto* j = dynamic_cast<DistanceJoint*>(component)) {
        AddJointProperties(b,j); b.Float("length",[j]{return j->GetLength();},[j](float v){j->SetLength(v);},0.0); b.Bool("enable_spring",[j]{return j->GetEnableSpring();},[j](bool v){j->SetEnableSpring(v);}); b.Float("hertz",[j]{return j->GetHertz();},[j](float v){j->SetHertz(v);},0.0); b.Float("damping_ratio",[j]{return j->GetDampingRatio();},[j](float v){j->SetDampingRatio(v);},0.0,1.0); b.Float("lower_spring_force",[j]{return j->GetLowerSpringForce();},[j](float v){j->SetLowerSpringForce(v);}); b.Float("upper_spring_force",[j]{return j->GetUpperSpringForce();},[j](float v){j->SetUpperSpringForce(v);}); b.Bool("enable_limit",[j]{return j->GetEnableLimit();},[j](bool v){j->SetEnableLimit(v);}); b.Float("min_length",[j]{return j->GetMinLength();},[j](float v){j->SetMinLength(v);},0.0); b.Float("max_length",[j]{return j->GetMaxLength();},[j](float v){j->SetMaxLength(v);},0.0); b.Bool("enable_motor",[j]{return j->GetEnableMotor();},[j](bool v){j->SetEnableMotor(v);}); b.Float("motor_speed",[j]{return j->GetMotorSpeed();},[j](float v){j->SetMotorSpeed(v);}); b.Float("max_motor_force",[j]{return j->GetMaxMotorForce();},[j](float v){j->SetMaxMotorForce(v);},0.0); b.Float("current_length",[j]{return j->GetCurrentLength();});
    } else if (auto* j = dynamic_cast<RevoluteJoint*>(component)) {
        AddJointProperties(b,j); b.Float("target_angle",[j]{return j->GetTargetAngle();},[j](float v){j->SetTargetAngle(v);}); b.Bool("enable_spring",[j]{return j->GetEnableSpring();},[j](bool v){j->SetEnableSpring(v);}); b.Float("hertz",[j]{return j->GetHertz();},[j](float v){j->SetHertz(v);},0.0); b.Float("damping_ratio",[j]{return j->GetDampingRatio();},[j](float v){j->SetDampingRatio(v);},0.0,1.0); b.Bool("enable_limit",[j]{return j->GetEnableLimit();},[j](bool v){j->SetEnableLimit(v);}); b.Float("lower_angle",[j]{return j->GetLowerAngle();},[j](float v){j->SetLowerAngle(v);}); b.Float("upper_angle",[j]{return j->GetUpperAngle();},[j](float v){j->SetUpperAngle(v);}); b.Bool("enable_motor",[j]{return j->GetEnableMotor();},[j](bool v){j->SetEnableMotor(v);}); b.Float("motor_speed",[j]{return j->GetMotorSpeed();},[j](float v){j->SetMotorSpeed(v);}); b.Float("max_motor_torque",[j]{return j->GetMaxMotorTorque();},[j](float v){j->SetMaxMotorTorque(v);},0.0); b.Float("current_angle",[j]{return j->GetAngle();});
    } else if (auto* j = dynamic_cast<PrismaticJoint*>(component)) {
        AddJointProperties(b,j); b.Float("axis_angle",[j]{return j->GetAxisAngle();},[j](float v){j->SetAxisAngle(v);}); b.Bool("enable_spring",[j]{return j->GetEnableSpring();},[j](bool v){j->SetEnableSpring(v);}); b.Float("hertz",[j]{return j->GetHertz();},[j](float v){j->SetHertz(v);},0.0); b.Float("damping_ratio",[j]{return j->GetDampingRatio();},[j](float v){j->SetDampingRatio(v);},0.0,1.0); b.Float("target_translation",[j]{return j->GetTargetTranslation();},[j](float v){j->SetTargetTranslation(v);}); b.Bool("enable_limit",[j]{return j->GetEnableLimit();},[j](bool v){j->SetEnableLimit(v);}); b.Float("lower_translation",[j]{return j->GetLowerTranslation();},[j](float v){j->SetLowerTranslation(v);}); b.Float("upper_translation",[j]{return j->GetUpperTranslation();},[j](float v){j->SetUpperTranslation(v);}); b.Bool("enable_motor",[j]{return j->GetEnableMotor();},[j](bool v){j->SetEnableMotor(v);}); b.Float("motor_speed",[j]{return j->GetMotorSpeed();},[j](float v){j->SetMotorSpeed(v);}); b.Float("max_motor_force",[j]{return j->GetMaxMotorForce();},[j](float v){j->SetMaxMotorForce(v);},0.0); b.Float("current_translation",[j]{return j->GetTranslation();});
    } else if (auto* j = dynamic_cast<WeldJoint*>(component)) {
        AddJointProperties(b,j); b.Float("linear_hertz",[j]{return j->GetLinearHertz();},[j](float v){j->SetLinearHertz(v);},0.0); b.Float("linear_damping_ratio",[j]{return j->GetLinearDampingRatio();},[j](float v){j->SetLinearDampingRatio(v);},0.0,1.0); b.Float("angular_hertz",[j]{return j->GetAngularHertz();},[j](float v){j->SetAngularHertz(v);},0.0); b.Float("angular_damping_ratio",[j]{return j->GetAngularDampingRatio();},[j](float v){j->SetAngularDampingRatio(v);},0.0,1.0);
    } else if (auto* j = dynamic_cast<WheelJoint*>(component)) {
        AddJointProperties(b,j); b.Float("axis_angle",[j]{return j->GetAxisAngle();},[j](float v){j->SetAxisAngle(v);}); b.Bool("enable_spring",[j]{return j->GetEnableSpring();},[j](bool v){j->SetEnableSpring(v);}); b.Float("hertz",[j]{return j->GetHertz();},[j](float v){j->SetHertz(v);},0.0); b.Float("damping_ratio",[j]{return j->GetDampingRatio();},[j](float v){j->SetDampingRatio(v);},0.0,1.0); b.Bool("enable_limit",[j]{return j->GetEnableLimit();},[j](bool v){j->SetEnableLimit(v);}); b.Float("lower_translation",[j]{return j->GetLowerTranslation();},[j](float v){j->SetLowerTranslation(v);}); b.Float("upper_translation",[j]{return j->GetUpperTranslation();},[j](float v){j->SetUpperTranslation(v);}); b.Bool("enable_motor",[j]{return j->GetEnableMotor();},[j](bool v){j->SetEnableMotor(v);}); b.Float("motor_speed",[j]{return j->GetMotorSpeed();},[j](float v){j->SetMotorSpeed(v);}); b.Float("max_motor_torque",[j]{return j->GetMaxMotorTorque();},[j](float v){j->SetMaxMotorTorque(v);},0.0);
    } else if (auto* j = dynamic_cast<MotorJoint*>(component)) {
        AddJointProperties(b,j); b.Vec2("linear_velocity",[j]{return j->GetLinearVelocity();},[j](glm::vec2 v){j->SetLinearVelocity(v);}); b.Float("max_velocity_force",[j]{return j->GetMaxVelocityForce();},[j](float v){j->SetMaxVelocityForce(v);},0.0); b.Float("angular_velocity",[j]{return j->GetAngularVelocity();},[j](float v){j->SetAngularVelocity(v);}); b.Float("max_velocity_torque",[j]{return j->GetMaxVelocityTorque();},[j](float v){j->SetMaxVelocityTorque(v);},0.0); b.Float("linear_hertz",[j]{return j->GetLinearHertz();},[j](float v){j->SetLinearHertz(v);},0.0); b.Float("linear_damping_ratio",[j]{return j->GetLinearDampingRatio();},[j](float v){j->SetLinearDampingRatio(v);},0.0,1.0); b.Float("max_spring_force",[j]{return j->GetMaxSpringForce();},[j](float v){j->SetMaxSpringForce(v);},0.0); b.Float("angular_hertz",[j]{return j->GetAngularHertz();},[j](float v){j->SetAngularHertz(v);},0.0); b.Float("angular_damping_ratio",[j]{return j->GetAngularDampingRatio();},[j](float v){j->SetAngularDampingRatio(v);},0.0,1.0); b.Float("max_spring_torque",[j]{return j->GetMaxSpringTorque();},[j](float v){j->SetMaxSpringTorque(v);},0.0);
    } else if (auto* p = dynamic_cast<ParticleComponent*>(component)) {
        b.Float("emission_rate",[p]{return p->GetEmissionRate();},[p](float v){p->SetEmissionRate(v);},0.0); b.Int("max_particles",[p]{return p->GetMaxParticles();},[p](int v){p->SetMaxParticles(v);},1); b.Float("duration",[p]{return p->GetDuration();},[p](float v){p->SetDuration(v);},0.0); b.Bool("looping",[p]{return p->GetLooping();},[p](bool v){p->SetLooping(v);}); b.Int("start_burst",[p]{return p->GetStartBurst();},[p](int v){p->SetStartBurst(v);},0); b.Vec2("lifetime",[p]{return p->GetLifetime();},[p](glm::vec2 v){p->SetLifetime(v);}); b.Vec2("speed",[p]{return p->GetSpeed();},[p](glm::vec2 v){p->SetSpeed(v);}); b.Float("direction",[p]{return p->GetDirection();},[p](float v){p->SetDirection(v);}); b.Float("spread",[p]{return p->GetSpread();},[p](float v){p->SetSpread(v);},0.0,360.0); b.Vec2("gravity",[p]{return p->GetGravity();},[p](glm::vec2 v){p->SetGravity(v);}); b.Float("damping",[p]{return p->GetDamping();},[p](float v){p->SetDamping(v);},0.0); b.Enum("space",[p]{return static_cast<int>(p->GetSpace());},[p](int v){p->SetSpace(static_cast<ParticleComponent::SimulationSpace>(v));},{"Local","World"}); b.Enum("shape",[p]{return static_cast<int>(p->GetShape());},[p](int v){p->SetShape(static_cast<ParticleComponent::Shape>(v));},{"Point","Circle","Box","Cone"}); b.Vec2("shape_size",[p]{return p->GetShapeSize();},[p](glm::vec2 v){p->SetShapeSize(v);}); b.Float("cone_angle",[p]{return p->GetConeAngle();},[p](float v){p->SetConeAngle(v);},0.0,180.0); b.String("sprite",[p]{return p->GetSpriteID();},[p](const std::string& v){p->SetSprite(v);},"Sprite"); b.Bool("flip_x",[p]{return p->GetFlipX();},[p](bool v){p->SetFlipX(v);}); b.Bool("flip_y",[p]{return p->GetFlipY();},[p](bool v){p->SetFlipY(v);}); b.Vec4("start_color",[p]{return p->GetStartColor();},[p](glm::vec4 v){p->SetStartColor(v);}); b.Vec4("end_color",[p]{return p->GetEndColor();},[p](glm::vec4 v){p->SetEndColor(v);}); b.Float("start_size",[p]{return p->GetStartSize();},[p](float v){p->SetStartSize(v);},0.0); b.Float("end_size",[p]{return p->GetEndSize();},[p](float v){p->SetEndSize(v);},0.0); b.Enum("blend_mode",[p]{return static_cast<int>(p->GetBlendMode());},[p](int v){p->SetBlendMode(static_cast<ParticleComponent::BlendMode>(v));},{"Alpha","Additive"}); if(sortingLayers.empty())b.String("sorting_layer",[p]{return p->GetSortingLayer();},[p](const std::string& v){p->SetSortingLayer(v);});else b.StringChoice("sorting_layer",[p]{return p->GetSortingLayer();},[p](const std::string& v){p->SetSortingLayer(v);},sortingLayers); b.Int("sorting_order",[p]{return p->GetSortingOrder();},[p](int v){p->SetSortingOrder(v);});
    } else if (auto* l = dynamic_cast<Light*>(component)) {
        b.Enum("type",[l]{return static_cast<int>(l->GetType());},[l](int v){l->SetType(static_cast<Light::LightType>(v));},{"Point","Spot","Directional","Global"}); b.Vec4("color",[l]{return l->GetColor();},[l](glm::vec4 v){l->SetColor(v);}); b.Float("intensity",[l]{return l->GetIntensity();},[l](float v){l->SetIntensity(v);},0.0); b.Float("range",[l]{return l->GetRange();},[l](float v){l->SetRange(v);},0.0); b.Float("inner_radius",[l]{return l->GetInnerRadius();},[l](float v){l->SetInnerRadius(v);},0.0,1.0); b.Float("falloff",[l]{return l->GetFalloff();},[l](float v){l->SetFalloff(v);},0.0); b.Float("inner_angle",[l]{return l->GetInnerAngle();},[l](float v){l->SetInnerAngle(v);},0.0,180.0); b.Float("outer_angle",[l]{return l->GetOuterAngle();},[l](float v){l->SetOuterAngle(v);},0.0,180.0); b.Float("height",[l]{return l->GetHeight();},[l](float v){l->SetHeight(v);},0.0); b.Float("normal_influence",[l]{return l->GetNormalInfluence();},[l](float v){l->SetNormalInfluence(v);},0.0,1.0); b.Bool("cast_shadows",[l]{return l->GetCastShadows();},[l](bool v){l->SetCastShadows(v);}); b.Float("shadow_strength",[l]{return l->GetShadowStrength();},[l](float v){l->SetShadowStrength(v);},0.0,1.0);
    } else if (auto* s = dynamic_cast<ShadowCaster*>(component)) {
        b.Enum("shape",[s]{return static_cast<int>(s->GetShape());},[s](int v){s->SetShape(static_cast<ShadowCaster::Shape>(v));},{"SpriteBounds","Box","Circle","FromCollider","SpriteAlpha"}); b.Vec2("center",[s]{return s->GetCenter();},[s](glm::vec2 v){s->SetCenter(v);}); b.Vec2("size",[s]{return s->GetSize();},[s](glm::vec2 v){s->SetSize(v);}); b.Float("radius",[s]{return s->GetRadius();},[s](float v){s->SetRadius(v);},0.0); b.Int("circle_segments",[s]{return s->GetCircleSegments();},[s](int v){s->SetCircleSegments(v);},3); b.Float("alpha_threshold",[s]{return s->GetAlphaThreshold();},[s](float v){s->SetAlphaThreshold(v);},0.0,1.0);
    } else if (auto* c = dynamic_cast<Camera*>(component)) {
        b.Enum("projection",[c]{return static_cast<int>(c->GetProjection());},[c](int v){c->SetProjection(static_cast<RenderCamera::Projection>(v));},{"Orthographic"}); b.Float("ortho_size",[c]{return c->GetOrthoSize();},[c](float v){c->SetOrthoSize(v);},0.01); b.Enum("clear_flags",[c]{return static_cast<int>(c->GetClearFlags());},[c](int v){c->SetClearFlags(static_cast<RenderCamera::ClearFlags>(v));},{"SolidColor","DepthOnly","Nothing"}); b.Vec4("clear_color",[c]{return c->GetClearColor();},[c](glm::vec4 v){c->SetClearColor(v);}); b.Int("priority",[c]{return c->GetPriority();},[c](int v){c->SetPriority(v);}); b.Float("target_aspect",[c]{return c->GetTargetAspect();},[c](float v){c->SetTargetAspect(v);}); b.Vec4("viewport_rect",[c]{return c->GetViewportRect();},[c](glm::vec4 v){c->SetViewportRect(v);}); b.StringList("culling_layers",[c]{return c->GetCullingLayers();},[c](std::vector<std::string> v){c->SetCullingLayers(std::move(v));}); b.String("target_texture",[c]{return c->GetTargetTextureID();},[c](const std::string& v){c->SetTargetTextureID(v);},"Texture2D");
    } else if (auto* a = dynamic_cast<Animator*>(component)) {
        b.String("default_state",[a]{return a->GetDefaultState();},[a](const std::string& v){a->SetDefaultState(v);}); b.String("current_state",[a]{return a->GetCurrentState();});
    } else if (auto* a = dynamic_cast<AudioSource*>(component)) {
        b.String("clip",[a]{return a->GetClipID();},[a](const std::string& v){a->SetClip(v);},"AudioClip"); b.Bool("play_on_awake",[a]{return a->GetPlayOnAwake();},[a](bool v){a->SetPlayOnAwake(v);}); b.Bool("loop",[a]{return a->GetLoop();},[a](bool v){a->SetLoop(v);}); b.Bool("mute",[a]{return a->GetMute();},[a](bool v){a->SetMute(v);}); b.Float("volume",[a]{return a->GetVolume();},[a](float v){a->SetVolume(v);},0.0,1.0); b.Float("pitch",[a]{return a->GetPitch();},[a](float v){a->SetPitch(v);},0.01); b.Float("spatial_blend",[a]{return a->GetSpatialBlend();},[a](float v){a->SetSpatialBlend(v);},0.0,1.0); b.Float("min_distance",[a]{return a->GetMinDistance();},[a](float v){a->SetMinDistance(v);},0.0); b.Float("max_distance",[a]{return a->GetMaxDistance();},[a](float v){a->SetMaxDistance(v);},0.0); b.Bool("is_playing",[a]{return a->IsPlaying();});
    } else if (dynamic_cast<AudioListener*>(component)) {
        // Component::enabled is the listener's only authored property.
    } else if (auto* s = dynamic_cast<ScriptComponent*>(component)) {
        b.String("script_module",[s]{return s->GetScriptModuleName();}); b.String("script_class",[s]{return s->GetScriptClassName();});
        const auto fields = s->GetFields();
        for (const ScriptFieldInfo& field : fields) {
            const QString key = "field." + QString::fromStdString(field.name);

            // Reflect[T, Options(...)] surfaces as an MCP enum carrying its choice
            // list, so an agent picks a name instead of guessing an int -- the
            // schema contract in Editor/CLAUDE.md calls out enum choices on dynamic
            // ScriptComponent fields specifically. A str field stores the label; an
            // int field stores the option's value (see ScriptFieldInfo).
            //
            // A list[Reflect[T, Options(...)]] is excluded: its choices describe
            // each element, so the field is still an array and belongs on the
            // generic path below -- describing it as a scalar enum would advertise
            // a get/set contract neither side can honour.
            if (!field.optionLabels.empty() && field.typeName != "list") {
                Property p; p.name=key; p.type="enum"; p.description=QString::fromStdString(field.tooltip);
                QStringList labels; std::vector<int> values;
                for (std::size_t i=0;i<field.optionLabels.size();++i) {
                    labels.push_back(QString::fromStdString(field.optionLabels[i]));
                    values.push_back(i<field.optionValues.size()?field.optionValues[i]:static_cast<int>(i));
                    p.choices.append(labels.back());
                }
                const std::string optionField=field.name;
                const bool byLabel=(field.typeName=="str");
                p.get=[s,optionField,labels,values,byLabel]()->QJsonValue{
                    ScriptFieldValue v=s->GetFieldValue(optionField);
                    if(byLabel) return std::holds_alternative<std::string>(v)?QJsonValue(QString::fromStdString(std::get<std::string>(v))):QJsonValue(QString());
                    const int cur=std::holds_alternative<int>(v)?std::get<int>(v):0;
                    for(std::size_t i=0;i<values.size();++i) if(values[i]==cur) return QJsonValue(labels[static_cast<int>(i)]);
                    return QJsonValue(cur);   // stored value matches no option -- report it raw rather than lie
                };
                if(field.readOnly) p.readOnly=true;
                else p.set=[s,optionField,labels,values,byLabel](const QJsonValue& value)->QString{
                    int index=-1;
                    if(value.isString()){for(int i=0;i<labels.size();++i) if(labels[i].compare(value.toString(),Qt::CaseInsensitive)==0){index=i;break;}}
                    else if(value.isDouble()&&!byLabel){const int raw=static_cast<int>(value.toDouble());for(std::size_t i=0;i<values.size();++i) if(values[i]==raw){index=static_cast<int>(i);break;}}
                    else return QString("expected one of: %1").arg(labels.join(", "));
                    if(index<0) return QString("unknown option; expected one of: %1").arg(labels.join(", "));
                    if(byLabel) s->SetFieldValue(optionField,labels[index].toStdString());
                    else        s->SetFieldValue(optionField,values[static_cast<std::size_t>(index)]);
                    return QString{};
                };
                b.Add(std::move(p));
                continue;
            }

            Property p; p.name=key; p.type=field.typeName=="list" ? "list["+QString::fromStdString(field.elementTypeName)+"]" : QString::fromStdString(field.typeName); p.referenceType=QString::fromStdString(field.refTypeName.empty()?field.elementRefTypeName:field.refTypeName); if(field.typeName=="float"||field.typeName=="int"){p.minimum=field.min;p.maximum=field.max;p.step=field.step;} p.description=QString::fromStdString(field.tooltip);
            const std::string fieldName=field.name;
            const QString referenceType=p.referenceType;
            const std::string fieldType=field.typeName;
            const float fieldMin=field.min, fieldMax=field.max;
            p.get=[s,fieldName]{ ScriptFieldValue v=s->GetFieldValue(fieldName); return std::visit([](const auto& x)->QJsonValue { using T=std::decay_t<decltype(x)>; if constexpr (kIsVector<T>) return VectorToJson(x); else return ToJson(x); },v); };
            // Reflect[T, ReadOnly()] fields stay describable but unwritable, so the
            // MCP schema matches what the Inspector allows (see this file's contract
            // in Editor/CLAUDE.md). Leaving p.set unassigned is what makes
            // set_component_property and the batch setter reject it.
            if(field.readOnly) p.readOnly=true; else p.set=[s,fieldName,referenceType,fieldType,fieldMin,fieldMax](const QJsonValue& value){ ScriptFieldValue current=s->GetFieldValue(fieldName); QString error; ScriptFieldValue parsed=std::visit([&](const auto& old)->ScriptFieldValue { using T=std::decay_t<decltype(old)>; T n{}; if constexpr(std::is_same_v<T,bool>) error=ParseBool(value,n); else if constexpr(std::is_same_v<T,int>) error=ParseInt(value,n); else if constexpr(std::is_same_v<T,float>) error=ParseFloat(value,n); else if constexpr(std::is_same_v<T,std::string>) { error=ParseString(value,n); if(error.isEmpty())error=ValidateReference(referenceType,n); } else if constexpr(std::is_same_v<T,glm::vec2>) error=ParseVec<2>(value,n); else if constexpr(std::is_same_v<T,glm::vec3>) error=ParseVec<3>(value,n); else if constexpr(std::is_same_v<T,glm::vec4>) error=ParseVec<4>(value,n); else { if(!value.isArray()){error="expected an array";return n;} for(const QJsonValue& x:value.toArray()){typename T::value_type item{}; if constexpr(std::is_same_v<typename T::value_type,bool>) error=ParseBool(x,item); else if constexpr(std::is_same_v<typename T::value_type,int>) error=ParseInt(x,item); else if constexpr(std::is_same_v<typename T::value_type,float>) error=ParseFloat(x,item); else if constexpr(std::is_same_v<typename T::value_type,glm::vec2>) error=ParseVec<2>(x,item); else if constexpr(std::is_same_v<typename T::value_type,glm::vec3>) error=ParseVec<3>(x,item); else if constexpr(std::is_same_v<typename T::value_type,glm::vec4>) error=ParseVec<4>(x,item); else {error=ParseString(x,item);if(error.isEmpty())error=ValidateReference(referenceType,item);} if(!error.isEmpty()) break; n.push_back(item);} } return n; },current); if(error.isEmpty()&&fieldType=="float"){float n=std::get<float>(parsed);if(n<fieldMin||n>fieldMax)error=QString("must be between %1 and %2").arg(fieldMin).arg(fieldMax);} if(error.isEmpty()&&fieldType=="int"){int n=std::get<int>(parsed);if(n<fieldMin||n>fieldMax)error=QString("must be between %1 and %2").arg(fieldMin).arg(fieldMax);} if(error.isEmpty()) s->SetFieldValue(fieldName,parsed); return error; };
            // PropertyBag intentionally keeps storage private; use the generic insertion hook below.
            b.Add(std::move(p));
        }
    }
    return b;
}

struct ResolvedAsset {
    Resource* resource = nullptr;
    QString type;
};

ResolvedAsset FindAsset(const std::string& id) {
    AssetManager& assets = AssetManager::Get();
    if (auto* v = assets.GetSprite(id)) return {v, "Sprite"};
    if (auto* v = assets.GetMaterial(id)) return {v, "Material"};
    if (auto* v = assets.GetTexture(id)) return {v, "Texture2D"};
    if (auto* v = assets.GetFont(id)) return {v, "Font"};
    if (auto* v = assets.GetAudioClip(id)) return {v, "AudioClip"};
    if (auto* v = assets.GetShader(id)) return {v, "Shader"};
    return {};
}

PropertyBag BuildAssetBag(Resource* resource) {
    PropertyBag b;
    b.String("name", [resource]{ return resource->GetName(); });
    b.String("path", [resource]{ return resource->GetFilePath(); });

    if (auto* s = dynamic_cast<Sprite*>(resource)) {
        b.Vec2("uv_min", [s]{ return s->GetUVMin(); }, [s](glm::vec2 v){ s->SetUVMin(v); });
        b.Vec2("uv_max", [s]{ return s->GetUVMax(); }, [s](glm::vec2 v){ s->SetUVMax(v); });
        b.Vec2("pivot", [s]{ return s->GetPivot(); }, [s](glm::vec2 v){ s->SetPivot(v); });
        b.String("texture", [s]{ return s->GetTextureID(); }, [s](const std::string& v){ std::string id=v; s->SetTexture(id); }, "Texture2D");
        b.Vec2("pixel_size", [s]{ return s->GetPixelSize(); });
    } else if (auto* m = dynamic_cast<Material*>(resource)) {
        b.String("shader", [m]{ return ResourceId(m->GetShader()); }, [m](const std::string& v){ std::string id=v; m->SetShader(id); }, "Shader");
        // Match the Inspector: the shader is authoritative. This exposes active
        // uniforms even before the material has stored an override for them.
        if (Shader* shader = m->GetShader()) for (const auto& [name, info] : shader->GetActiveUniforms()) {
            const QString key = "uniform." + QString::fromStdString(name);
            switch (info.type) {
                case GL_FLOAT: b.Float(key,[m,name]{return m->GetFloat(name);},[m,name](float v){m->SetFloat(name,v);}); break;
                case GL_FLOAT_VEC2: b.Vec2(key,[m,name]{return m->GetVec2(name);},[m,name](glm::vec2 v){m->SetVec2(name,v);}); break;
                case GL_FLOAT_VEC3: b.Vec3(key,[m,name]{return m->GetVec3(name);},[m,name](glm::vec3 v){m->SetVec3(name,v);}); break;
                case GL_FLOAT_VEC4: b.Vec4(key,[m,name]{return m->GetVec4(name);},[m,name](glm::vec4 v){m->SetVec4(name,v);}); break;
                case GL_SAMPLER_2D: b.String(key,[m,name]{return m->GetTexUniform(name);},[m,name](const std::string& v){m->SetTexture(name,v);},"Texture2D"); break;
                default: break;
            }
        }
    } else if (auto* t = dynamic_cast<Texture2D*>(resource)) {
        b.Int("width", [t]{return t->GetWidth();}); b.Int("height", [t]{return t->GetHeight();});
        b.Enum("filter",[t]{return static_cast<int>(t->GetFilter());},[t](int v){t->SetFilter(static_cast<TextureFilter>(v));},{"Nearest","Linear"});
        b.Enum("wrap",[t]{return static_cast<int>(t->GetWrap());},[t](int v){t->SetWrap(static_cast<TextureWrap>(v));},{"Repeat","Clamp"});
        b.Bool("apply_normal",[t]{return t->GetApplyNormal();},[t](bool v){t->SetApplyNormal(v);});
        b.Enum("normal_height_source",[t]{return static_cast<int>(t->GetNormalHeightSource());},[t](int v){t->SetNormalHeightSource(static_cast<NormalHeightSource>(v));},{"Luminance","Alpha"});
        b.Enum("normal_edge_filter",[t]{return static_cast<int>(t->GetNormalEdgeFilter());},[t](int v){t->SetNormalEdgeFilter(static_cast<NormalEdgeFilter>(v));},{"Sobel","Scharr"});
        b.Float("normal_strength",[t]{return t->GetNormalStrength();},[t](float v){t->SetNormalStrength(v);},0.0);
        b.Int("normal_blur",[t]{return t->GetNormalBlur();},[t](int v){t->SetNormalBlur(v);},0);
        b.Bool("normal_invert_x",[t]{return t->GetNormalInvertX();},[t](bool v){t->SetNormalInvertX(v);});
        b.Bool("normal_invert_y",[t]{return t->GetNormalInvertY();},[t](bool v){t->SetNormalInvertY(v);});
    } else if (auto* f = dynamic_cast<Font*>(resource)) {
        b.String("source",[f]{return f->GetSourcePath();});
        b.Int("atlas_em_size",[f]{return f->GetEmSize();},[f](int v){f->SetEmSize(v);},8);
        b.Float("distance_range",[f]{return f->GetPxRange();},[f](float v){f->SetPxRange(v);},0.1);
        b.String("charset",[f]{return f->GetCharset();},[f](const std::string& v){f->SetCharset(v);});
        b.Bool("atlas_ready",[f]{return f->IsReady();}); b.Int("atlas_width",[f]{return f->GetAtlasWidth();}); b.Int("atlas_height",[f]{return f->GetAtlasHeight();});
    } else if (auto* a = dynamic_cast<AudioClip*>(resource)) {
        b.String("source",[a]{return a->GetPath();}); b.Float("duration",[a]{return a->GetDuration();}); b.Int("channels",[a]{return a->GetChannels();}); b.Int("sample_rate",[a]{return a->GetSampleRate();});
    } else if (auto* s = dynamic_cast<Shader*>(resource)) {
        b.Int("program_id",[s]{return static_cast<int>(s->GetProgramID());});
        b.Enum("domain",[s]{return static_cast<int>(s->GetDomain());}, {}, {"Sprite","Text"});
    }
    return b;
}

McpResult ResolveComponent(const QJsonObject& params, Component** out, Container* container = nullptr) {
    const QString id = params.value("componentId").toString(params.value("id").toString());
    if (id.isEmpty()) return McpResult::Error(InvalidParams, "missing \"componentId\"");
    container = container ? container : support::ActiveContainer();
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    Component* component = registry ? registry->Find<Component>(id.toStdString()) : nullptr;
    if (!component) return McpResult::Error(ObjectNotFound, "no Component with id " + id);
    *out = component;
    return McpResult::Ok();
}

McpResult ApplyComponentProperty(Container* container, const std::string& id,
                                 const QString& propertyName, const QJsonValue& value) {
    Registry* registry = container ? container->FindSystem<Registry>() : nullptr;
    Component* component = registry ? registry->Find<Component>(id) : nullptr;
    if (!component) return McpResult::Error(ObjectNotFound, "no Component with id " + QString::fromStdString(id));
    PropertyBag bag = BuildComponentBag(component);
    Property* property = bag.Find(propertyName);
    if (!property) return McpResult::Error(InvalidParams, "component type " + QString::fromStdString(component->GetTypeName()) + " has no property \"" + propertyName + "\"");
    if (property->readOnly || !property->set) return McpResult::Error(InvalidParams, "property \"" + propertyName + "\" is read-only");
    const QString error = property->set(value);
    if (!error.isEmpty()) return McpResult::Error(InvalidParams, "invalid value for \"" + propertyName + "\": " + error);
    return McpResult::Ok(property->get());
}

class McpPropertyCommand final : public Command {
public:
    McpPropertyCommand(std::string componentId, QString property, QJsonValue before, QJsonValue after)
        : m_componentId(std::move(componentId)), m_property(std::move(property)),
          m_before(std::move(before)), m_after(std::move(after)) {}
    void Undo(Container* container) override { ApplyComponentProperty(container,m_componentId,m_property,m_before); }
    void Redo(Container* container) override { ApplyComponentProperty(container,m_componentId,m_property,m_after); }
    std::string GetText() const override { return "Change " + DisplayName(m_property).toStdString(); }
private:
    std::string m_componentId;
    QString m_property;
    QJsonValue m_before;
    QJsonValue m_after;
};

McpResult SetComponentProperties(const QJsonObject& params) {
    Component* component = nullptr;
    if (McpResult r = ResolveComponent(params, &component); !r.ok) return r;
    if (!params.value("values").isObject()) return McpResult::Error(InvalidParams, "missing object \"values\"");

    PropertyBag bag = BuildComponentBag(component);
    struct Change { QString name; QJsonValue before; QJsonValue after; Property* property; };
    std::vector<Change> changes;
    const QJsonObject requested = params.value("values").toObject();
    for (auto it=requested.begin(); it!=requested.end(); ++it) {
        Property* property=bag.Find(it.key());
        if(!property) return McpResult::Error(InvalidParams,"component type "+QString::fromStdString(component->GetTypeName())+" has no property \""+it.key()+"\"");
        if(property->readOnly||!property->set) return McpResult::Error(InvalidParams,"property \""+it.key()+"\" is read-only");
        changes.push_back({it.key(),property->get(),it.value(),property});
    }

    std::size_t applied=0;
    for (; applied<changes.size(); ++applied) {
        const QString error=changes[applied].property->set(changes[applied].after);
        if(!error.isEmpty()) {
            const QString failed = changes[applied].name;
            while(applied>0){--applied; changes[applied].property->set(changes[applied].before);}
            return McpResult::Error(InvalidParams,"invalid value for \""+failed+"\": "+error+"; no changes were kept");
        }
        changes[applied].after=changes[applied].property->get();
    }

    Container* container=support::ActiveContainer();
    if(container) if(auto* undo=container->FindSystem<UndoSystem>()) {
        std::vector<std::unique_ptr<Command>> commands;
        for(const Change& change:changes) if(change.before!=change.after)
            commands.push_back(std::make_unique<McpPropertyCommand>(component->GetID(),change.name,change.before,change.after));
        if(auto command=MacroCommand::Wrap(std::move(commands),"Change "+component->GetTypeName())) undo->Push(std::move(command));
    }

    QJsonObject result; result["componentId"]=QString::fromStdString(component->GetID()); result["type"]=QString::fromStdString(component->GetTypeName());
    QJsonObject values; for(const Change& change:changes) values[change.name]=change.after; result["values"]=values; result["undoable"]=true;
    return McpResult::Ok(result);
}

McpResult SetAssetProperties(const QJsonObject& params) {
    const QString id=params.value("assetId").toString(params.value("id").toString());
    if(id.isEmpty()) return McpResult::Error(InvalidParams,"missing \"assetId\"");
    ResolvedAsset resolved=FindAsset(id.toStdString());
    if(!resolved.resource) return McpResult::Error(ObjectNotFound,"no loaded asset with id "+id);
    if(!params.value("values").isObject()) return McpResult::Error(InvalidParams,"missing object \"values\"");
    PropertyBag bag=BuildAssetBag(resolved.resource);
    struct Change{QString name;QJsonValue before;QJsonValue after;Property* property;}; std::vector<Change> changes;
    const QJsonObject requested=params.value("values").toObject();
    for(auto it=requested.begin();it!=requested.end();++it){Property* p=bag.Find(it.key());if(!p)return McpResult::Error(InvalidParams,resolved.type+" has no property \""+it.key()+"\"");if(p->readOnly||!p->set)return McpResult::Error(InvalidParams,"property \""+it.key()+"\" is read-only");changes.push_back({it.key(),p->get(),it.value(),p});}
    std::size_t applied=0; for(;applied<changes.size();++applied){QString e=changes[applied].property->set(changes[applied].after);if(!e.isEmpty()){const QString failed=changes[applied].name;while(applied>0){--applied;changes[applied].property->set(changes[applied].before);}return McpResult::Error(InvalidParams,"invalid value for \""+failed+"\": "+e+"; no changes were kept");}changes[applied].after=changes[applied].property->get();}
    QJsonObject values;for(const Change& c:changes)values[c.name]=c.after;QJsonObject result{{"assetId",id},{"type",resolved.type},{"values",values},{"undoable",false}};return McpResult::Ok(result);
}

QJsonObject ComponentDescription(Component* component) {
    PropertyBag bag=BuildComponentBag(component); QJsonObject result;
    result["id"]=QString::fromStdString(component->GetID()); result["type"]=QString::fromStdString(component->GetTypeName()); result["enabled"]=component->GetEnabled();
    if(GameObject* owner=component->GetGameObject()){result["gameObjectId"]=QString::fromStdString(owner->GetID());result["gameObjectName"]=QString::fromStdString(owner->GetName());}
    result["properties"]=bag.Describe(); result["serializedState"]=YamlToJson(component->Serialize()); return result;
}

// Describes what ComponentActions::For reports, so the schema an agent sees is
// generated from the same registry the Inspector's buttons and an Event's method
// dropdown read. This used to be a hardcoded dynamic_cast chain duplicating that
// knowledge, with nothing keeping the two in agreement -- and it could not see a
// script's @action methods at all.
QJsonArray ComponentActionSchema(Component* component) {
    QJsonArray actions;
    for(const ComponentActionInfo& a:ComponentActions::For(component)){
        QJsonObject entry{{"name",QString::fromStdString(a.name)}};
        if(!a.label.empty())   entry["label"]=QString::fromStdString(a.label);
        if(!a.tooltip.empty()) entry["description"]=QString::fromStdString(a.tooltip);
        if(a.hasArg){
            QJsonObject arg{{"name",QString::fromStdString(a.arg.name)},
                            {"type",QString::fromStdString(a.arg.typeName)}};
            if(!a.arg.refTypeName.empty()) arg["referenceType"]=QString::fromStdString(a.arg.refTypeName);
            entry["arguments"]=QJsonArray{arg};
        }
        actions.append(entry);
    }
    // Not a runtime action -- reassigning which script a component runs is an
    // authoring operation, so it stays declared here rather than in the registry
    // the event system draws from.
    if(dynamic_cast<ScriptComponent*>(component)) actions.append(QJsonObject{{"name","set_script"},{"arguments",QJsonArray{QJsonObject{{"name","module"},{"type","string"}},QJsonObject{{"name","class"},{"type","string"}}}}});
    return actions;
}

// Flatten one JSON argument into the string form ComponentActions::Invoke parses.
QString RawArgFromJson(const QJsonValue& v) {
    if(v.isUndefined()||v.isNull()) return {};
    if(v.isString()) return v.toString();
    if(v.isBool())   return v.toBool()?"true":"false";
    if(v.isDouble()) return QString::number(v.toDouble());
    if(v.isArray()){QStringList parts;for(const QJsonValue& x:v.toArray())parts<<QString::number(x.toDouble());return parts.join(QChar(','));}
    return {};
}

McpResult InvokeComponentAction(const QJsonObject& params) {
    Component* component=nullptr;if(McpResult r=ResolveComponent(params,&component);!r.ok)return r;
    const QString action=params.value("action").toString();const QJsonObject args=params.value("arguments").toObject();

    // Authoring-only, and not part of the shared registry -- see above.
    if(auto* s=dynamic_cast<ScriptComponent*>(component);s&&action=="set_script"){const QString module=args.value("module").toString(),klass=args.value("class").toString();if(module.isEmpty()||klass.isEmpty())return McpResult::Error(InvalidParams,"set_script requires module and class");s->SetScript(module.toStdString(),klass.toStdString());return McpResult::Ok(QJsonObject{{"module",module},{"class",klass}});}

    // Animator's set_parameter is typed by the VALUE rather than by the action, so
    // it does not fit the registry's one-declared-argument shape and keeps its own
    // handler.
    if(auto* a=dynamic_cast<Animator*>(component);a&&action=="set_parameter"){const std::string name=args.value("name").toString().toStdString();const QJsonValue v=args.value("value");if(v.isBool())a->SetBool(name,v.toBool());else if(v.isDouble()&&std::floor(v.toDouble())==v.toDouble())a->SetInt(name,v.toInt());else if(v.isDouble())a->SetFloat(name,static_cast<float>(v.toDouble()));else return McpResult::Error(InvalidParams,"Animator parameter value must be bool, int or float");return McpResult::Ok(QJsonObject{{"action",action}});}

    // Everything else routes through the shared registry, which means a script's
    // @action methods are invokable over MCP for free.
    QString raw;
    for(const ComponentActionInfo& info:ComponentActions::For(component)){
        if(info.name!=action.toStdString()) continue;
        if(info.hasArg){
            QJsonValue v=args.value(QString::fromStdString(info.arg.name));
            if(v.isUndefined()) v=args.value("value");
            raw=RawArgFromJson(v);
        }
        break;
    }
    std::string error;
    if(!ComponentActions::Invoke(component,action.toStdString(),raw.toStdString(),&error))
        return McpResult::Error(InvalidParams,QString::fromStdString(error));
    return McpResult::Ok(QJsonObject{{"action",action}});
}

} // namespace

void RegisterPropertyTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("capabilities.get", [](const QJsonObject&) {
        QJsonArray types; for(const std::string& name:SerializableFactory::GetRegisteredTypeNames()) types.append(QString::fromStdString(name));
        QJsonObject result{{"protocolVersion",2},{"componentAddressing","exact-id"},{"componentTypes",types}};
        result["tools"]=QJsonArray{"component.list","component.describe","component.get_property","component.set_property","component.set_properties","component.invoke","asset.describe","asset.get_property","asset.set_property","asset.set_properties","asset.save","reference.resolve","console.get_messages","scene.capture_view","user.request_clarification","user.clarification_status"};
        result["transactions"]="component.set_properties and asset.set_properties validate and roll back as one batch";result["componentUndo"]=true;result["assetUndo"]=false;result["destructiveClarification"]=QJsonArray{"object.destroy","object.remove_component"};result["clarificationOtherAlwaysIncluded"]=true;result["clarificationMultiSelect"]=true;return McpResult::Ok(result);
    });
    dispatcher.RegisterTool("component.list", [](const QJsonObject& params){GameObject* object=nullptr;if(McpResult r=support::ResolveGameObject(QJsonObject{{"id",params.value("objectId").toString(params.value("id").toString())}},&object);!r.ok)return r;QJsonArray items;for(Component* c:object->GetAllComponents())if(c)items.append(ComponentDescription(c));return McpResult::Ok(QJsonObject{{"objectId",QString::fromStdString(object->GetID())},{"components",items}});});
    dispatcher.RegisterTool("component.describe", [](const QJsonObject& params){Component* c=nullptr;if(McpResult r=ResolveComponent(params,&c);!r.ok)return r;QJsonObject d=ComponentDescription(c);d["actions"]=ComponentActionSchema(c);return McpResult::Ok(d);});
    dispatcher.RegisterTool("component.get_property", [](const QJsonObject& params){Component* c=nullptr;if(McpResult r=ResolveComponent(params,&c);!r.ok)return r;const QString name=params.value("property").toString();PropertyBag b=BuildComponentBag(c);Property* p=b.Find(name);if(!p)return McpResult::Error(InvalidParams,"unknown property \""+name+"\"");return McpResult::Ok(QJsonObject{{"componentId",QString::fromStdString(c->GetID())},{"property",name},{"value",p->get()},{"schema",p->Describe(false)}});});
    dispatcher.RegisterTool("component.set_property", [](const QJsonObject& params){if(!params.contains("property")||!params.contains("value"))return McpResult::Error(InvalidParams,"set_property requires property and value");QJsonObject batch=params;batch["values"]=QJsonObject{{params.value("property").toString(),params.value("value")}};return SetComponentProperties(batch);});
    dispatcher.RegisterTool("component.set_properties", SetComponentProperties);
    dispatcher.RegisterTool("component.invoke", InvokeComponentAction);
    dispatcher.RegisterTool("asset.describe", [](const QJsonObject& params){const QString id=params.value("assetId").toString(params.value("id").toString());ResolvedAsset a=FindAsset(id.toStdString());if(!a.resource)return McpResult::Error(ObjectNotFound,"no loaded asset with id "+id);PropertyBag b=BuildAssetBag(a.resource);return McpResult::Ok(QJsonObject{{"id",id},{"type",a.type},{"name",QString::fromStdString(a.resource->GetName())},{"path",QString::fromStdString(a.resource->GetFilePath())},{"properties",b.Describe()},{"serializedState",YamlToJson(a.resource->Serialize())}});});
    dispatcher.RegisterTool("asset.get_property", [](const QJsonObject& params){const QString id=params.value("assetId").toString(params.value("id").toString());ResolvedAsset a=FindAsset(id.toStdString());if(!a.resource)return McpResult::Error(ObjectNotFound,"no loaded asset with id "+id);const QString name=params.value("property").toString();PropertyBag b=BuildAssetBag(a.resource);Property* p=b.Find(name);if(!p)return McpResult::Error(InvalidParams,"unknown property \""+name+"\"");return McpResult::Ok(QJsonObject{{"assetId",id},{"property",name},{"value",p->get()},{"schema",p->Describe(false)}});});
    dispatcher.RegisterTool("asset.set_property", [](const QJsonObject& params){if(!params.contains("property")||!params.contains("value"))return McpResult::Error(InvalidParams,"set_property requires property and value");QJsonObject batch=params;batch["values"]=QJsonObject{{params.value("property").toString(),params.value("value")}};return SetAssetProperties(batch);});
    dispatcher.RegisterTool("asset.set_properties", SetAssetProperties);
    dispatcher.RegisterTool("asset.save", [](const QJsonObject& params){const QString id=params.value("assetId").toString(params.value("id").toString());ResolvedAsset a=FindAsset(id.toStdString());if(!a.resource)return McpResult::Error(ObjectNotFound,"no loaded asset with id "+id);AssetManager::Get().SaveResource(a.resource);return McpResult::Ok(QJsonObject{{"assetId",id},{"path",QString::fromStdString(a.resource->GetFilePath())},{"saved",true}});});
    dispatcher.RegisterTool("reference.resolve", [](const QJsonObject& params){const QString id=params.value("id").toString();if(id.isEmpty())return McpResult::Error(InvalidParams,"missing \"id\"");if(GameObject* object=support::FindGameObject(id.toStdString()))return McpResult::Ok(QJsonObject{{"kind","GameObject"},{"id",id},{"name",QString::fromStdString(object->GetName())},{"tag",QString::fromStdString(object->GetTag())}});Registry* registry=support::ActiveRegistry();if(Component* c=registry?registry->Find<Component>(id.toStdString()):nullptr)return McpResult::Ok(ComponentDescription(c));ResolvedAsset a=FindAsset(id.toStdString());if(a.resource)return McpResult::Ok(QJsonObject{{"kind","Asset"},{"type",a.type},{"id",id},{"name",QString::fromStdString(a.resource->GetName())},{"path",QString::fromStdString(a.resource->GetFilePath())}});return McpResult::Error(ObjectNotFound,"no object, component, or loaded asset with id "+id);});
}

} // namespace mcp
