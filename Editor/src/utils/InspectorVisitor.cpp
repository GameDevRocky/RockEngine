#include "utils/InspectorVisitor.hpp"
#include <algorithm>
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/core/LayerManager.hpp"
#include "engine/core/TagManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "Engine.hpp"
#include "utils/ComponentPickerWidget.hpp"
#include "dock-widgets/SpriteEditorModal.hpp"
#include <QLabel>
#include <QPixmap>
#include <QSizePolicy>
#include <QPushButton>
#include <map>

using namespace Properties;

InspectorVisitor::InspectorVisitor(){
    content = new QWidget();
    layout = new QGridLayout();
    layout->setContentsMargins(0,0,0,0);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    content->setLayout(layout);
}


void InspectorVisitor::Visit(GameObject* obj){
    

    TagManager* tagManager = Engine::Get()->GetActiveContainer()->FindSystem<TagManager>();
    
        std::vector<std::pair<std::string, std::any>> tagOptions;
        const auto& allTags = tagManager->GetTags();
        for (int i = 0; i < static_cast<int>(allTags.size()); ++i)
            tagOptions.push_back({ allTags[i], i });

        auto tag_get = [=]() -> int {
            const auto& t = tagManager->GetTags();
            auto it = std::find(t.begin(), t.end(), obj->GetTag());
            return it != t.end() ? static_cast<int>(it - t.begin()) : 0;
        };
        auto tag_set = [=](int idx) {
            const auto& t = tagManager->GetTags();
            if (idx >= 0 && idx < static_cast<int>(t.size()))
                obj->SetTag(t[idx]);
        };

        BindProperty<int>(obj, "Tag: ", tag_get, tag_set,
            obj->TAG_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals(tagOptions));
}

void InspectorVisitor::Visit(Transform* transform){

    auto pos_get = [=](){
        if (!transform) return glm::vec2(0.0f);
        return transform->localPosition;
    };  

    auto pos_set = [=](glm::vec2 pos){
        if (!transform) return;
        transform->SetPosition(pos);
    };  

    auto rot_get = [=](){
        if (!transform) return 0.0f;
        return transform->localRotation;
    };  

    auto rot_set = [=](float val){
        if (!transform) return;
        transform->SetRotation(val);
    }; 
    
    auto scale_get = [=](){
        if (!transform) return glm::vec2(0.0f);
        return transform->localScale;
    };  

    auto scale_set = [=](glm::vec2 pos){
        if (!transform) return;
        transform->SetScale(pos);
    };  

    BindProperty<glm::vec2>(transform, "Position: ", pos_get, pos_set, transform->POSITION_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty<float>(transform, "Rotation: ", rot_get, rot_set, transform->ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));
    BindProperty<glm::vec2>(transform, "Scale: ", scale_get, scale_set, transform->SCALE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    
    
}

void InspectorVisitor::Visit(SpriteRenderer* renderer){

    auto color_get = [=](){
        return renderer->GetColor();
    };
    auto color_set = [=](glm::vec4 color){
        renderer->SetColor(color);
    };
    auto flipX_get = [=](){
        return renderer->GetFlipX();
    };
    auto flipX_set = [=](bool val){
        renderer->SetFlipX(val);
    };
    auto flipY_get = [=](){
        return renderer->GetFlipY();
    };
    auto flipY_set = [=](bool val){
        renderer->SetFlipY(val);
    };
    
    auto visible_get = [=](){
        return renderer->GetVisible();
    };

    auto visible_set = [=](bool val){
        renderer->SetVisible(val);
    };

    auto material_get = [=](){
        auto* mat = renderer->GetMaterial();
        return mat? mat->GetID() : "";
    };

    auto material_set = [=](std::string val){
        renderer->SetMaterial(val);
    };
    auto sprite_get = [=](){
        auto* sprite = renderer->GetSprite();
        return sprite? sprite->GetID() : "";
    };

    auto sprite_set = [=](std::string val){
        renderer->SetSprite(val);
    };

    BindProperty<glm::vec4>(renderer, "Color: ", color_get, color_set, renderer->COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));
    BindProperty<bool>(renderer, "Flip X: ", flipX_get, flipX_set, renderer->FLIP_X_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<bool>(renderer, "Flip Y: ", flipY_get, flipY_set, renderer->FLIP_Y_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<bool>(renderer, "Visible: ", visible_get, visible_set, renderer->VISIBILITY_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<std::string>(renderer, "Material: ", material_get, material_set, renderer->MATERIAL_CHANGED_EVENT, PropDesc().Tag(Tags::MATERIAL).RefType(Tags::OBJECT_REF));
    BindProperty<std::string>(renderer, "Sprite: ", sprite_get, sprite_set, renderer->SPRITE_CHANGED_EVENT, PropDesc().Tag(Tags::SPRITE).RefType(Tags::OBJECT_REF));
    
    LayerManager* layerManager = Engine::Get()->GetActiveContainer()->FindSystem<LayerManager>();
    if (layerManager)
    {
        std::vector<std::pair<std::string, std::any>> layerOptions;
        for (const auto& layer : layerManager->GetLayers())
            layerOptions.push_back({ layer.name, layer.priority });

        auto layer_get = [=]() -> int {
            return layerManager->GetPriority(renderer->GetSortingLayer());
        };
        auto layer_set = [=](int priority) {
            for (const auto& layer : layerManager->GetLayers())
            {
                if (layer.priority == priority)
                {
                    renderer->SetSortingLayer(layer.name);
                    return;
                }
            }
        };

        BindProperty<int>(renderer, "Sorting Layer: ", layer_get, layer_set,
            renderer->SORTING_LAYER_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals(layerOptions));
    }

    auto order_get = [=]() -> float { return static_cast<float>(renderer->GetSortingOrder()); };
    auto order_set = [=](float val) { renderer->SetSortingOrder(static_cast<int>(val)); };
    BindProperty<float>(renderer, "Order in Layer: ", order_get, order_set,
        renderer->SORTING_ORDER_CHANGED_EVENT,
        PropDesc().Tag(Tags::INT).Range(-32768, 32767).Step(1));
    
}

void InspectorVisitor::Visit(Collider* collider){
    auto setCenter = [=](glm::vec2 val){
        collider->SetCenter(val);
    };
    auto getCenter = [=](){
        return collider->GetCenter();
    };

    auto setDensity = [=](float val){
        collider->SetDensity(val);
    };

    auto getDensity = [=](){
        return collider->GetDensity();
    };

    auto setBounciness = [=](float val){
        collider->SetBounciness(val);
    };

    auto getBounciness = [=](){
        return collider->GetBounciness();
    };

    auto setIsSensor = [=](bool val){
        collider->SetIsSensor(val);
    };

    auto getIsSensor = [=](){
        return collider->GetIsSensor();
    };

    auto setFriction = [=](float val){
        collider->SetFriction(val);
    };

    auto getFriction = [=](){
        return collider->GetFriction();
    };

    auto setRollingResistance = [=](float val){
        collider->SetRollingResistance(val);
    };

    auto getRollingResistance = [=](){
        return collider->GetRollingResistance();
    };

    BindProperty<glm::vec2>(collider, "Center: ", getCenter, setCenter, collider->CENTER_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2));
    BindProperty<float>(collider, "Density: ", getDensity, setDensity, collider->DENSITY_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(collider, "Bounciness: ", getBounciness, setBounciness, collider->BOUNCINESS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 1));
    BindProperty<bool>(collider, "IsSensor: ", getIsSensor, setIsSensor, collider->IS_SENSOR_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(collider, "Friction: ", getFriction, setFriction, collider->FRICTION_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 1));
    BindProperty<float>(collider, "Rolling Resistance: ", getRollingResistance, setRollingResistance, collider->ROLLING_RESISTANCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 1));
}


void InspectorVisitor::Visit(BoxCollider* boxCollider){
    Visit(static_cast<Collider*>(boxCollider));
    auto getSize = [=](){
        return boxCollider->GetSize();
    };
    auto setSize = [=](glm::vec2 size){
        boxCollider->SetSize(size);
    };
   
    BindProperty<glm::vec2>(boxCollider, "Size: ", getSize, setSize, boxCollider->SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));    
}

void InspectorVisitor::Visit(CircleCollider* circleCollider){
    Visit(static_cast<Collider*>(circleCollider));
    auto getRadius = [=](){
        return circleCollider->GetRadius();
    };
    auto setRadius = [=](float radius){
        circleCollider->SetRadius(radius);
    };
    
    BindProperty<float>(circleCollider, "Radius: ", getRadius, setRadius, circleCollider->RADIUS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
}

void InspectorVisitor::Visit(CapsuleCollider* capsuleCollider){
    Visit(static_cast<Collider*>(capsuleCollider));
    auto getRadius = [=](){
        return capsuleCollider->GetRadius();
    };
    auto setRadius = [=](float radius){
        capsuleCollider->SetRadius(radius);
    };
    auto getHeight = [=](){
        return capsuleCollider->GetHeight();
    };
    auto setHeight = [=](float radius){
        capsuleCollider->SetHeight(radius);
    };
    
    BindProperty<float>(capsuleCollider, "Height: ", getHeight, setHeight, capsuleCollider->HEIGHT_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
    BindProperty<float>(capsuleCollider, "Radius: ", getRadius, setRadius, capsuleCollider->RADIUS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
}

void InspectorVisitor::Visit(RigidBody* rb){
    auto getUseGravity = [=](){
        return rb->GetUseGravity();
    };
    auto setUseGravity = [=](bool val){
        rb->SetUseGravity(val);
    };
    auto getLockRotation = [=](){
        return rb->GetLockRotation();
    };
    auto setLockRotation = [=](bool val){
        rb->SetLockRotation(val);
    };
    auto getBodyType = [=]() -> int {
        return static_cast<int>(rb->GetBodyType());
    };
    auto setBodyType = [=](int type){
        rb->SetBodyType(static_cast<b2BodyType>(type));
    };

    BindProperty<bool>(rb, "Use Gravity: ", getUseGravity, setUseGravity, rb->USE_GRAVITY_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<bool>(rb, "Lock Rotation: ", getLockRotation, setLockRotation, rb->LOCK_ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<int>(rb, "Body Type: ", getBodyType, setBodyType, rb->BODY_TYPE_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
        {"Dynamic",   static_cast<int>(b2_dynamicBody)},
        {"Kinematic", static_cast<int>(b2_kinematicBody)},
        {"Static",    static_cast<int>(b2_staticBody)}
    }));
}

void InspectorVisitor::Visit(ScriptComponent* sc){
    // Script selector — always the first row, so an unassigned ScriptComponent
    // still shows a way to pick a class. Opens a searchable popup listing every
    // ScriptableComponent subclass; picking one reassigns the script live.
    {
        std::string current = sc->GetScriptClassName();
        auto* scriptButton = new QPushButton(
            QString::fromStdString(current.empty() ? "(none)" : current));
        scriptButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QObject::connect(scriptButton, &QPushButton::clicked, scriptButton, [sc, scriptButton]() {
            // Build display names, disambiguating a class shared across modules
            // as "Class (module)". Map each back to its module/class pair.
            auto available = ScriptComponent::GetAvailableScripts();
            std::map<std::string, int> classCounts;
            for (const auto& s : available) classCounts[s.className]++;

            std::vector<std::string> display;
            std::map<std::string, ScriptClassInfo> byDisplay;
            for (const auto& s : available) {
                std::string name = (classCounts[s.className] > 1)
                    ? s.className + " (" + s.moduleName + ")"
                    : s.className;
                display.push_back(name);
                byDisplay[name] = s;
            }

            auto* picker = new ComponentPickerWidget(std::move(display), scriptButton);
            picker->setFixedWidth(scriptButton->width());
            picker->onSelected = [sc, byDisplay](const std::string& chosen) {
                auto it = byDisplay.find(chosen);
                if (it != byDisplay.end())
                    sc->SetScript(it->second.moduleName, it->second.className);
            };
            picker->move(scriptButton->mapToGlobal(QPoint(0, scriptButton->height())));
            picker->show();
        });

        AddRow("Script", scriptButton);
    }

    const auto& fields = sc->GetFields();

    // Fetch all field values in a single GIL acquisition to avoid N separate
    // GIL acquire/release round-trips blocking the main thread on selection.
    auto allValues = sc->GetAllFieldValues();

    for (const auto& field : fields) {
        std::string label = field.name + ": ";
        auto it = allValues.find(field.name);

        if (field.typeName == "float") {
            float initial = (it != allValues.end() && std::holds_alternative<float>(it->second))
                ? std::get<float>(it->second) : 0.0f;
            auto getter = [sc, name = field.name]() -> float {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<float>(val) ? std::get<float>(val) : 0.0f;
            };
            auto setter = [sc, name = field.name](float v) {
                sc->SetFieldValue(name, v);
            };
            BindProperty<float>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::FLOAT).Range(field.min, field.max).Step(field.step), initial);
        }
        else if (field.typeName == "int") {
            float initial = (it != allValues.end() && std::holds_alternative<int>(it->second))
                ? static_cast<float>(std::get<int>(it->second)) : 0.0f;
            auto getter = [sc, name = field.name]() -> float {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<int>(val) ? static_cast<float>(std::get<int>(val)) : 0.0f;
            };
            auto setter = [sc, name = field.name](float v) {
                sc->SetFieldValue(name, static_cast<int>(v));
            };
            BindProperty<float>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::FLOAT).Range(field.min, field.max).Step(1), initial);
        }
        else if (field.typeName == "bool") {
            bool initial = (it != allValues.end() && std::holds_alternative<bool>(it->second))
                ? std::get<bool>(it->second) : false;
            auto getter = [sc, name = field.name]() -> bool {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<bool>(val) ? std::get<bool>(val) : false;
            };
            auto setter = [sc, name = field.name](bool v) {
                sc->SetFieldValue(name, v);
            };
            BindProperty<bool>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::TOGGLE), initial);
        }
        else if (field.typeName == "str") {
            std::string initial = (it != allValues.end() && std::holds_alternative<std::string>(it->second))
                ? std::get<std::string>(it->second) : "";
            auto getter = [sc, name = field.name]() -> std::string {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<std::string>(val) ? std::get<std::string>(val) : "";
            };
            auto setter = [sc, name = field.name](std::string v) {
                sc->SetFieldValue(name, v);
            };

            // Build PropDesc based on optional ref type declared in the Python annotation
            PropDesc strDesc;
            if (field.refTypeName == "material") {
                strDesc = PropDesc().Tag(Tags::MATERIAL).RefType(Tags::OBJECT_REF);
            } else if (field.refTypeName == "sprite") {
                strDesc = PropDesc().Tag(Tags::SPRITE).RefType(Tags::OBJECT_REF);
            } else if (field.refTypeName.rfind("gameobject:", 0) == 0) {
                std::string cls = field.refTypeName.substr(std::string("gameobject:").size());
                strDesc = PropDesc().Tag(Tags::OBJECT_REF).RefType(Tags::OBJECT_REF).RefClass(cls);
            }
            // else: plain string — PropDesc defaults produce a StringPropertyWidget

            BindProperty<std::string>(sc, label, getter, setter, field.changeEvent,
                strDesc, initial);
        }
        else if (field.typeName == "vec2") {
            glm::vec2 initial = (it != allValues.end() && std::holds_alternative<glm::vec2>(it->second))
                ? std::get<glm::vec2>(it->second) : glm::vec2(0.0f);
            auto getter = [sc, name = field.name]() -> glm::vec2 {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<glm::vec2>(val) ? std::get<glm::vec2>(val) : glm::vec2(0.0f);
            };
            auto setter = [sc, name = field.name](glm::vec2 v) {
                sc->SetFieldValue(name, v);
            };
            BindProperty<glm::vec2>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::VECTOR2).Step(field.step), initial);
        }
        else if (field.typeName == "vec3") {
            glm::vec3 initial = (it != allValues.end() && std::holds_alternative<glm::vec3>(it->second))
                ? std::get<glm::vec3>(it->second) : glm::vec3(0.0f);
            auto getter = [sc, name = field.name]() -> glm::vec3 {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<glm::vec3>(val) ? std::get<glm::vec3>(val) : glm::vec3(0.0f);
            };
            auto setter = [sc, name = field.name](glm::vec3 v) {
                sc->SetFieldValue(name, v);
            };
            BindProperty<glm::vec3>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::VECTOR3).Step(field.step), initial);
        }
        else if (field.typeName == "vec4") {
            glm::vec4 initial = (it != allValues.end() && std::holds_alternative<glm::vec4>(it->second))
                ? std::get<glm::vec4>(it->second) : glm::vec4(0.0f);
            auto getter = [sc, name = field.name]() -> glm::vec4 {
                auto val = sc->GetFieldValue(name);
                return std::holds_alternative<glm::vec4>(val) ? std::get<glm::vec4>(val) : glm::vec4(0.0f);
            };
            auto setter = [sc, name = field.name](glm::vec4 v) {
                sc->SetFieldValue(name, v);
            };
            BindProperty<glm::vec4>(sc, label, getter, setter, field.changeEvent,
                PropDesc().Tag(Tags::VECTOR4).Step(field.step), initial);
        }
        else if (field.typeName == "list") {
            if (field.elementTypeName == "bool") {
                auto getter = [sc, name = field.name]() -> std::vector<bool> {
                    auto val = sc->GetFieldValue(name);
                    return std::holds_alternative<std::vector<bool>>(val)
                        ? std::get<std::vector<bool>>(val) : std::vector<bool>{};
                };
                auto setter = [sc, name = field.name](std::vector<bool> v) {
                    sc->SetFieldValue(name, v);
                };
                BindProperty<std::vector<bool>>(sc, label, getter, setter, field.changeEvent,
                    PropDesc().Tag(Tags::LIST).Element(PropDesc().Tag(Tags::TOGGLE)));
            }
            else if (field.elementTypeName == "float") {
                auto getter = [sc, name = field.name]() -> std::vector<float> {
                    auto val = sc->GetFieldValue(name);
                    return std::holds_alternative<std::vector<float>>(val)
                        ? std::get<std::vector<float>>(val) : std::vector<float>{};
                };
                auto setter = [sc, name = field.name](std::vector<float> v) {
                    sc->SetFieldValue(name, v);
                };
                BindProperty<std::vector<float>>(sc, label, getter, setter, field.changeEvent,
                    PropDesc().Tag(Tags::LIST).Element(PropDesc().Tag(Tags::FLOAT).Step(field.step)));
            }
            else if (field.elementTypeName == "int") {
                // Int lists are edited with float rows (like scalar ints), converting
                // at the boundary; the variant/Python side stays std::vector<int>.
                auto getter = [sc, name = field.name]() -> std::vector<float> {
                    auto val = sc->GetFieldValue(name);
                    std::vector<float> out;
                    if (std::holds_alternative<std::vector<int>>(val))
                        for (int x : std::get<std::vector<int>>(val)) out.push_back(static_cast<float>(x));
                    return out;
                };
                auto setter = [sc, name = field.name](std::vector<float> v) {
                    std::vector<int> out;
                    out.reserve(v.size());
                    for (float x : v) out.push_back(static_cast<int>(x));
                    sc->SetFieldValue(name, out);
                };
                BindProperty<std::vector<float>>(sc, label, getter, setter, field.changeEvent,
                    PropDesc().Tag(Tags::LIST).Element(PropDesc().Tag(Tags::FLOAT).Step(1)));
            }
            else if (field.elementTypeName == "str") {
                auto getter = [sc, name = field.name]() -> std::vector<std::string> {
                    auto val = sc->GetFieldValue(name);
                    return std::holds_alternative<std::vector<std::string>>(val)
                        ? std::get<std::vector<std::string>>(val) : std::vector<std::string>{};
                };
                auto setter = [sc, name = field.name](std::vector<std::string> v) {
                    sc->SetFieldValue(name, v);
                };

                // Element descriptor mirrors the scalar str path (plain / asset ref / GO ref).
                PropDesc elemDesc;
                if (field.elementRefTypeName == "material") {
                    elemDesc = PropDesc().Tag(Tags::MATERIAL).RefType(Tags::OBJECT_REF);
                } else if (field.elementRefTypeName == "sprite") {
                    elemDesc = PropDesc().Tag(Tags::SPRITE).RefType(Tags::OBJECT_REF);
                } else if (field.elementRefTypeName.rfind("gameobject:", 0) == 0) {
                    std::string cls = field.elementRefTypeName.substr(std::string("gameobject:").size());
                    elemDesc = PropDesc().Tag(Tags::OBJECT_REF).RefType(Tags::OBJECT_REF).RefClass(cls);
                }

                BindProperty<std::vector<std::string>>(sc, label, getter, setter, field.changeEvent,
                    PropDesc().Tag(Tags::LIST).Element(elemDesc));
            }
        }
    }
}


void InspectorVisitor::AddRow(const std::string& text, QWidget* widget){
    QLabel* label = new QLabel(text.c_str());
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    layout->addWidget(label, gridRow, 0, Qt::AlignLeft);
    layout->addWidget(widget, gridRow, 1);
    gridRow++;
}

void InspectorVisitor::AddFullRow(QWidget* widget) {
    layout->addWidget(widget, gridRow, 0, 1, 2);
    gridRow++;
}

void InspectorVisitor::Visit(Sprite* sprite) {
    auto uvMin_get = [=]() { return sprite->GetUVMin(); };
    auto uvMin_set = [=](glm::vec2 v) { sprite->SetUVMin(v); };
    auto uvMax_get = [=]() { return sprite->GetUVMax(); };
    auto uvMax_set = [=](glm::vec2 v) { sprite->SetUVMax(v); };
    auto pivot_get = [=]() { return sprite->GetPivot(); };
    auto pivot_set = [=](glm::vec2 v) { sprite->SetPivot(v); };
    auto tex_get   = [=]() -> std::string {
        auto* t = sprite->GetTexture();
        return t ? t->GetID() : "";
    };
    auto tex_set   = [=](std::string id) { sprite->SetTexture(id); };

    BindProperty<glm::vec2>(sprite, "UV Min: ",   uvMin_get,  uvMin_set,  sprite->UV_MIN_CHANGED_EVENT,  PropDesc().Tag(Tags::VECTOR2).Step(0.01).Range(0, 1));
    BindProperty<glm::vec2>(sprite, "UV Max: ",   uvMax_get,  uvMax_set,  sprite->UV_MAX_CHANGED_EVENT,  PropDesc().Tag(Tags::VECTOR2).Step(0.01).Range(0, 1));
    BindProperty<glm::vec2>(sprite, "Pivot: ",    pivot_get,  pivot_set,  sprite->PIVOT_CHANGED_EVENT,   PropDesc().Tag(Tags::VECTOR2).Step(0.01).Range(0, 1));
    BindProperty<std::string>(sprite, "Texture: ", tex_get,   tex_set,    sprite->UV_MAX_CHANGED_EVENT,  PropDesc().Tag(Tags::TEXTURE).RefType(Tags::OBJECT_REF));
}

void InspectorVisitor::Visit(Material* mat) {
    auto shader_get = [=]() -> std::string {
        auto* s = mat->GetShader();
        return s ? s->GetID() : "";
    };
    auto shader_set = [=](std::string id) { mat->SetShader(id); };
    BindProperty<std::string>(mat, "Shader: ", shader_get, shader_set, mat->SHADER_CHANGED_EVENT, PropDesc().Tag(Tags::SHADER).RefType(Tags::OBJECT_REF));

    Shader* shader = mat->GetShader();
    if (!shader) return;

    auto isColorName = [](const std::string& n) {
        std::string low = n;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        return low.find("color") != std::string::npos || low.find("colour") != std::string::npos;
    };

    // Drive widgets from the shader's active uniforms so they always reflect the
    // current shader — even for uniforms the material hasn't stored a value for yet.
    // mat4 (uModel/uView/uProj) are skipped by the default branch.
    // Getters fall back to zero if the uniform isn't in the material's map yet;
    // the first edit stores it there via the normal setter path.
    for (const auto& [uname, info] : shader->GetActiveUniforms()) {
        switch (info.type) {
            case GL_FLOAT: {
                auto get = [mat, name = uname]() { return mat->GetFloat(name); };
                auto set = [mat, name = uname](float v) { mat->SetFloat(name, v); };
                BindProperty<float>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT));
                break;
            }
            case GL_FLOAT_VEC2: {
                auto get = [mat, name = uname]() { return mat->GetVec2(name); };
                auto set = [mat, name = uname](glm::vec2 v) { mat->SetVec2(name, v); };
                BindProperty<glm::vec2>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2));
                break;
            }
            case GL_FLOAT_VEC3: {
                auto get = [mat, name = uname]() { return mat->GetVec3(name); };
                auto set = [mat, name = uname](glm::vec3 v) { mat->SetVec3(name, v); };
                BindProperty<glm::vec3>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR3));
                break;
            }
            case GL_FLOAT_VEC4: {
                Tags widgetTag = isColorName(uname) ? Tags::COLOR : Tags::VECTOR4;
                auto get = [mat, name = uname]() { return mat->GetVec4(name); };
                auto set = [mat, name = uname](glm::vec4 v) { mat->SetVec4(name, v); };
                BindProperty<glm::vec4>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(widgetTag));
                break;
            }
            case GL_SAMPLER_2D: {
                auto get = [mat, name = uname]() { return mat->GetTexUniform(name); };
                auto set = [mat, name = uname](std::string id) { mat->SetTexture(name, id); };
                BindProperty<std::string>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::TEXTURE).RefType(Tags::OBJECT_REF));
                break;
            }
            default:
                break;
        }
    }
}

void InspectorVisitor::Visit(Texture2D* tex) {
    // Image preview — spans both columns, same load path as AssetPreviewDelegate.
    auto* preview = new QLabel();
    preview->setAlignment(Qt::AlignCenter);
    preview->setFixedHeight(180);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    preview->setStyleSheet("background-color: #1a1a1a; border: 1px solid #3a3a3a;");

    const std::string& imgPath = tex->GetPath();
    if (!imgPath.empty()) {
        QPixmap px(QString::fromStdString(imgPath));
        if (!px.isNull())
            preview->setPixmap(px.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    AddFullRow(preview);

    // Open the interactive sprite editor for this texture (a centered, click-away modal).
    auto* editSpritesBtn = new QPushButton("Edit Sprites");
    QObject::connect(editSpritesBtn, &QPushButton::clicked, editSpritesBtn, [tex]() {
        auto* modal = new SpriteEditorModal(tex);
        modal->show();
        modal->raise();
        modal->activateWindow();
    });
    AddFullRow(editSpritesBtn);

    auto path_get = [=]() { return tex->GetPath(); };
    auto w_get    = [=]() { return static_cast<float>(tex->GetWidth()); };
    auto h_get    = [=]() { return static_cast<float>(tex->GetHeight()); };

    BindProperty<std::string>(tex, "Path: ",   path_get, [](std::string){}, Observable::CreateEvent(), PropDesc().Tag(Tags::READONLY));
    BindProperty<float>(tex,       "Width: ",  w_get,    [](float){},       Observable::CreateEvent(), PropDesc().Tag(Tags::FLOAT));
    BindProperty<float>(tex,       "Height: ", h_get,    [](float){},       Observable::CreateEvent(), PropDesc().Tag(Tags::FLOAT));

    auto filter_get = [=]() { return static_cast<int>(tex->GetFilter()); };
    auto filter_set = [=](int v) { tex->SetFilter(static_cast<TextureFilter>(v)); };
    BindProperty<int>(tex, "Filtering: ", filter_get, filter_set, tex->FILTER_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Nearest", static_cast<int>(TextureFilter::Nearest)},
            {"Linear",  static_cast<int>(TextureFilter::Linear)}
        }));

    auto wrap_get = [=]() { return static_cast<int>(tex->GetWrap()); };
    auto wrap_set = [=](int v) { tex->SetWrap(static_cast<TextureWrap>(v)); };
    BindProperty<int>(tex, "Wrap: ", wrap_get, wrap_set, tex->WRAP_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Repeat", static_cast<int>(TextureWrap::Repeat)},
            {"Clamp",  static_cast<int>(TextureWrap::Clamp)}
        }));
}

void InspectorVisitor::Visit(Shader* shader) {
    auto id_get = [=]() { return shader->GetID(); };

    BindProperty<std::string>(shader, "ID: ", id_get, [](std::string){}, Observable::CreateEvent(), PropDesc().Tag(Tags::READONLY));
}
