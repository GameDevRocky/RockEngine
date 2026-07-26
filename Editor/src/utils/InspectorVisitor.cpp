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
#include "engine/components/Camera.hpp"
#include "engine/components/Animator.hpp"
#include "engine/components/ParticleComponent.hpp"
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
#include "utils/TexturePreviewWidget.hpp"
#include "dock-widgets/SpriteEditorModal.hpp"
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/AnimatorGui.hpp"
#include <QLabel>
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
        // Captures the tag names by value rather than the TagManager, so the
        // setter stays pointer-free and an undo resolves the same tag it recorded
        // even if the tag list has since been edited.
        auto tag_set = [tags = allTags](GameObject* go, const int& idx) {
            if (idx >= 0 && idx < static_cast<int>(tags.size()))
                go->SetTag(tags[idx]);
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

    auto pos_set = [](Transform* t, const glm::vec2& pos){
        t->SetPosition(pos);
    };

    auto rot_get = [=](){
        if (!transform) return 0.0f;
        return transform->localRotation;
    };

    auto rot_set = [](Transform* t, const float& val){
        t->SetRotation(val);
    };

    auto scale_get = [=](){
        if (!transform) return glm::vec2(0.0f);
        return transform->localScale;
    };

    auto scale_set = [](Transform* t, const glm::vec2& scale){
        t->SetScale(scale);
    };

    BindProperty<glm::vec2>(transform, "Position: ", pos_get, pos_set, transform->POSITION_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty<float>(transform, "Rotation: ", rot_get, rot_set, transform->ROTATION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));
    BindProperty<glm::vec2>(transform, "Scale: ", scale_get, scale_set, transform->SCALE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    
    
}

void InspectorVisitor::Visit(SpriteRenderer* renderer){

    auto color_get = [=](){
        return renderer->GetColor();
    };
    auto color_set = [](SpriteRenderer* r, const glm::vec4& color){
        r->SetColor(color);
    };
    auto flipX_get = [=](){
        return renderer->GetFlipX();
    };
    auto flipX_set = [](SpriteRenderer* r, const bool& val){
        r->SetFlipX(val);
    };
    auto flipY_get = [=](){
        return renderer->GetFlipY();
    };
    auto flipY_set = [](SpriteRenderer* r, const bool& val){
        r->SetFlipY(val);
    };

    auto visible_get = [=](){
        return renderer->GetVisible();
    };

    // SetVisible/SetMaterial/SetSprite take a non-const reference, so copy into a
    // mutable local before calling.
    auto visible_set = [](SpriteRenderer* r, const bool& val){
        bool v = val;
        r->SetVisible(v);
    };

    auto material_get = [=](){
        auto* mat = renderer->GetMaterial();
        return mat? mat->GetID() : "";
    };

    auto material_set = [](SpriteRenderer* r, const std::string& val){
        std::string v = val;
        r->SetMaterial(v);
    };
    auto sprite_get = [=](){
        auto* sprite = renderer->GetSprite();
        return sprite? sprite->GetID() : "";
    };

    auto sprite_set = [](SpriteRenderer* r, const std::string& val){
        std::string v = val;
        r->SetSprite(v);
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
        // Captures the priority->name pairs by value so the setter is pointer-free.
        auto layer_set = [layers = layerOptions](SpriteRenderer* r, const int& priority) {
            for (const auto& [name, prio] : layers)
            {
                if (std::any_cast<int>(prio) == priority)
                {
                    r->SetSortingLayer(name);
                    return;
                }
            }
        };

        BindProperty<int>(renderer, "Sorting Layer: ", layer_get, layer_set,
            renderer->SORTING_LAYER_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals(layerOptions));
    }

    auto order_get = [=]() -> float { return static_cast<float>(renderer->GetSortingOrder()); };
    auto order_set = [](SpriteRenderer* r, const float& val) { r->SetSortingOrder(static_cast<int>(val)); };
    BindProperty<float>(renderer, "Order in Layer: ", order_get, order_set,
        renderer->SORTING_ORDER_CHANGED_EVENT,
        PropDesc().Tag(Tags::INT).Range(-32768, 32767).Step(1));
    
}

void InspectorVisitor::Visit(Collider* collider){
    auto setCenter = [](Collider* c, const glm::vec2& val){
        c->SetCenter(val);
    };
    auto getCenter = [=](){
        return collider->GetCenter();
    };

    auto setDensity = [](Collider* c, const float& val){
        c->SetDensity(val);
    };

    auto getDensity = [=](){
        return collider->GetDensity();
    };

    auto setBounciness = [](Collider* c, const float& val){
        c->SetBounciness(val);
    };

    auto getBounciness = [=](){
        return collider->GetBounciness();
    };

    auto setIsSensor = [](Collider* c, const bool& val){
        c->SetIsSensor(val);
    };

    auto getIsSensor = [=](){
        return collider->GetIsSensor();
    };

    auto setFriction = [](Collider* c, const float& val){
        c->SetFriction(val);
    };

    auto getFriction = [=](){
        return collider->GetFriction();
    };

    auto setRollingResistance = [](Collider* c, const float& val){
        c->SetRollingResistance(val);
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
    auto setSize = [](BoxCollider* c, const glm::vec2& size){
        c->SetSize(size);
    };
   
    BindProperty<glm::vec2>(boxCollider, "Size: ", getSize, setSize, boxCollider->SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));    
}

void InspectorVisitor::Visit(CircleCollider* circleCollider){
    Visit(static_cast<Collider*>(circleCollider));
    auto getRadius = [=](){
        return circleCollider->GetRadius();
    };
    auto setRadius = [](CircleCollider* c, const float& radius){
        c->SetRadius(radius);
    };
    
    BindProperty<float>(circleCollider, "Radius: ", getRadius, setRadius, circleCollider->RADIUS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
}

void InspectorVisitor::Visit(CapsuleCollider* capsuleCollider){
    Visit(static_cast<Collider*>(capsuleCollider));
    auto getRadius = [=](){
        return capsuleCollider->GetRadius();
    };
    auto setRadius = [](CapsuleCollider* c, const float& radius){
        c->SetRadius(radius);
    };
    auto getHeight = [=](){
        return capsuleCollider->GetHeight();
    };
    auto setHeight = [](CapsuleCollider* c, const float& height){
        c->SetHeight(height);
    };
    
    BindProperty<float>(capsuleCollider, "Height: ", getHeight, setHeight, capsuleCollider->HEIGHT_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
    BindProperty<float>(capsuleCollider, "Radius: ", getRadius, setRadius, capsuleCollider->RADIUS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
}

void InspectorVisitor::Visit(RigidBody* rb){
    auto getUseGravity = [=](){
        return rb->GetUseGravity();
    };
    auto setUseGravity = [](RigidBody* b, const bool& val){
        b->SetUseGravity(val);
    };
    auto getLockRotation = [=](){
        return rb->GetLockRotation();
    };
    auto setLockRotation = [](RigidBody* b, const bool& val){
        b->SetLockRotation(val);
    };
    auto getBodyType = [=]() -> int {
        return static_cast<int>(rb->GetBodyType());
    };
    auto setBodyType = [](RigidBody* b, const int& type){
        b->SetBodyType(static_cast<b2BodyType>(type));
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
    // still shows a way to pick a class. Uses the same asset-picker widget as
    // Sprite/Material/GameObject refs: a "…" picker listing every
    // ScriptableComponent subclass, plus drag-and-drop of a .py script file from
    // the Folder view. The value is carried as "module:class" (see the SCRIPT tag
    // handling in ObjectRefPropertyWidget / RefDropFilter).
    {
        auto* pw = new ObjectRefPropertyWidget(
            Properties::PropDesc().Tag(Properties::Tags::SCRIPT));

        std::string current;
        if (!sc->GetScriptClassName().empty())
            current = sc->GetScriptModuleName() + ":" + sc->GetScriptClassName();
        pw->SetValue(current);

        pw->onChanged = [sc](const std::string& ref) {
            // ref is "module:class"; split and reassign the script live. SetScript
            // fires SCRIPT_RELOADED_EVENT, which rebuilds this inspector (queued),
            // so it's safe to run from the widget's own callback.
            const auto colon = ref.find(':');
            if (colon == std::string::npos) return;
            sc->SetScript(ref.substr(0, colon), ref.substr(colon + 1));
        };

        AddRow("Script", pw->GetWidget());
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
            auto setter = [name = field.name](ScriptComponent* sc, const float& v) {
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
            auto setter = [name = field.name](ScriptComponent* sc, const float& v) {
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
            auto setter = [name = field.name](ScriptComponent* sc, const bool& v) {
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
            auto setter = [name = field.name](ScriptComponent* sc, const std::string& v) {
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
            } else if (field.refTypeName.rfind("component:", 0) == 0) {
                std::string comp = field.refTypeName.substr(std::string("component:").size());
                strDesc = PropDesc().Tag(Tags::OBJECT_REF).RefType(Tags::OBJECT_REF).ComponentType(comp);
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
            auto setter = [name = field.name](ScriptComponent* sc, const glm::vec2& v) {
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
            auto setter = [name = field.name](ScriptComponent* sc, const glm::vec3& v) {
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
            auto setter = [name = field.name](ScriptComponent* sc, const glm::vec4& v) {
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
                auto setter = [name = field.name](ScriptComponent* sc, const std::vector<bool>& v) {
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
                auto setter = [name = field.name](ScriptComponent* sc, const std::vector<float>& v) {
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
                auto setter = [name = field.name](ScriptComponent* sc, const std::vector<float>& v) {
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
                auto setter = [name = field.name](ScriptComponent* sc, const std::vector<std::string>& v) {
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
                } else if (field.elementRefTypeName.rfind("component:", 0) == 0) {
                    std::string comp = field.elementRefTypeName.substr(std::string("component:").size());
                    elemDesc = PropDesc().Tag(Tags::OBJECT_REF).RefType(Tags::OBJECT_REF).ComponentType(comp);
                }

                BindProperty<std::vector<std::string>>(sc, label, getter, setter, field.changeEvent,
                    PropDesc().Tag(Tags::LIST).Element(elemDesc));
            }
        }
    }
}


void InspectorVisitor::Visit(ParticleComponent* p) {
    using PC = ParticleComponent;

    // ── Emission ──
    BindProperty<float>(p, "Emission Rate: ",
        [=]() { return p->GetEmissionRate(); },
        [](PC* e, const float& v) { e->SetEmissionRate(v); },
        p->EMISSION_RATE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10000).Step(1));

    BindProperty<float>(p, "Max Particles: ",
        [=]() { return static_cast<float>(p->GetMaxParticles()); },
        [](PC* e, const float& v) { e->SetMaxParticles(static_cast<int>(v)); },
        p->MAX_PARTICLES_CHANGED_EVENT, PropDesc().Tag(Tags::INT).Range(1, 1000000).Step(1));

    BindProperty<float>(p, "Duration: ",
        [=]() { return p->GetDuration(); },
        [](PC* e, const float& v) { e->SetDuration(v); },
        p->DURATION_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 100000).Step(0.1f));

    BindProperty<bool>(p, "Looping: ",
        [=]() { return p->GetLooping(); },
        [](PC* e, const bool& v) { e->SetLooping(v); },
        p->LOOPING_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    BindProperty<float>(p, "Start Burst: ",
        [=]() { return static_cast<float>(p->GetStartBurst()); },
        [](PC* e, const float& v) { e->SetStartBurst(static_cast<int>(v)); },
        p->START_BURST_CHANGED_EVENT, PropDesc().Tag(Tags::INT).Range(0, 1000000).Step(1));

    // ── Lifetime / motion ── (vec2 = min,max)
    BindProperty<glm::vec2>(p, "Lifetime (min,max): ",
        [=]() { return p->GetLifetime(); },
        [](PC* e, const glm::vec2& v) { e->SetLifetime(v); },
        p->LIFETIME_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Range(0, 100000).Step(0.1f));

    BindProperty<glm::vec2>(p, "Speed (min,max): ",
        [=]() { return p->GetSpeed(); },
        [](PC* e, const glm::vec2& v) { e->SetSpeed(v); },
        p->SPEED_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(0.1f));

    BindProperty<float>(p, "Direction: ",
        [=]() { return p->GetDirection(); },
        [](PC* e, const float& v) { e->SetDirection(v); },
        p->DIRECTION_CHANGED_EVENT, PropDesc().Tag(Tags::ANGLE).Step(1));

    BindProperty<float>(p, "Spread: ",
        [=]() { return p->GetSpread(); },
        [](PC* e, const float& v) { e->SetSpread(v); },
        p->SPREAD_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 360).Step(1));

    BindProperty<glm::vec2>(p, "Gravity: ",
        [=]() { return p->GetGravity(); },
        [](PC* e, const glm::vec2& v) { e->SetGravity(v); },
        p->GRAVITY_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(0.1f));

    BindProperty<float>(p, "Damping: ",
        [=]() { return p->GetDamping(); },
        [](PC* e, const float& v) { e->SetDamping(v); },
        p->DAMPING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 50).Step(0.1f));

    BindProperty<int>(p, "Simulation Space: ",
        [=]() { return static_cast<int>(p->GetSpace()); },
        [](PC* e, const int& v) { e->SetSpace(static_cast<PC::SimulationSpace>(v)); },
        p->SPACE_CHANGED_EVENT, PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Local", static_cast<int>(PC::SimulationSpace::Local)},
            {"World", static_cast<int>(PC::SimulationSpace::World)}
        }));

    // ── Shape ──
    BindProperty<int>(p, "Shape: ",
        [=]() { return static_cast<int>(p->GetShape()); },
        [](PC* e, const int& v) { e->SetShape(static_cast<PC::Shape>(v)); },
        p->SHAPE_CHANGED_EVENT, PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Point",  static_cast<int>(PC::Shape::Point)},
            {"Circle", static_cast<int>(PC::Shape::Circle)},
            {"Box",    static_cast<int>(PC::Shape::Box)},
            {"Cone",   static_cast<int>(PC::Shape::Cone)}
        }));

    BindProperty<glm::vec2>(p, "Shape Size (px): ",
        [=]() { return p->GetShapeSize(); },
        [](PC* e, const glm::vec2& v) { e->SetShapeSize(v); },
        p->SHAPE_SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));

    BindProperty<float>(p, "Cone Angle: ",
        [=]() { return p->GetConeAngle(); },
        [](PC* e, const float& v) { e->SetConeAngle(v); },
        p->CONE_ANGLE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 360).Step(1));

    // ── Appearance ──
    BindProperty<std::string>(p, "Sprite: ",
        [=]() { return p->GetSpriteID(); },
        [](PC* e, const std::string& v) { e->SetSprite(v); },
        p->SPRITE_CHANGED_EVENT, PropDesc().Tag(Tags::SPRITE).RefType(Tags::OBJECT_REF));

    BindProperty<glm::vec4>(p, "Start Color: ",
        [=]() { return p->GetStartColor(); },
        [](PC* e, const glm::vec4& v) { e->SetStartColor(v); },
        p->START_COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));

    BindProperty<glm::vec4>(p, "End Color: ",
        [=]() { return p->GetEndColor(); },
        [](PC* e, const glm::vec4& v) { e->SetEndColor(v); },
        p->END_COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));

    BindProperty<float>(p, "Start Size (px): ",
        [=]() { return p->GetStartSize(); },
        [](PC* e, const float& v) { e->SetStartSize(v); },
        p->START_SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 4096).Step(1));

    BindProperty<float>(p, "End Size (px): ",
        [=]() { return p->GetEndSize(); },
        [](PC* e, const float& v) { e->SetEndSize(v); },
        p->END_SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 4096).Step(1));

    BindProperty<int>(p, "Blend Mode: ",
        [=]() { return static_cast<int>(p->GetBlendMode()); },
        [](PC* e, const int& v) { e->SetBlendMode(static_cast<PC::BlendMode>(v)); },
        p->BLEND_MODE_CHANGED_EVENT, PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Alpha",    static_cast<int>(PC::BlendMode::Alpha)},
            {"Additive", static_cast<int>(PC::BlendMode::Additive)}
        }));

    BindProperty<float>(p, "Order in Layer: ",
        [=]() { return static_cast<float>(p->GetSortingOrder()); },
        [](PC* e, const float& v) { e->SetSortingOrder(static_cast<int>(v)); },
        p->SORTING_ORDER_CHANGED_EVENT, PropDesc().Tag(Tags::INT).Range(-32768, 32767).Step(1));

    // Manual one-shot burst for authoring/testing (explosions, puffs). Not
    // undoable -- it's a transient emission request, not a state change.
    auto* burstBtn = new QPushButton("Emit Burst");
    QObject::connect(burstBtn, &QPushButton::clicked, burstBtn, [p]() {
        int n = p->GetStartBurst() > 0 ? p->GetStartBurst() : 100;
        p->EmitBurst(n);
    });
    AddFullRow(burstBtn);
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

// Asset visitors (Sprite / Material / Texture2D / Shader) below.
//
// These are Serializables but not RuntimeObjects: they live in AssetManager, not
// in a container's Registry. BindProperty applies their edits through the
// inspected instance as usual, but the undo stack resolves targets through the
// registry, so assets never produce a command and are deliberately not undoable.
// Reverting an asset in memory without reverting the meta file it persists to
// would desync the two — that needs a file-level transaction story, not this.

void InspectorVisitor::Visit(Sprite* sprite) {
    auto uvMin_get = [=]() { return sprite->GetUVMin(); };
    auto uvMin_set = [](Sprite* s, const glm::vec2& v) { s->SetUVMin(v); };
    auto uvMax_get = [=]() { return sprite->GetUVMax(); };
    auto uvMax_set = [](Sprite* s, const glm::vec2& v) { s->SetUVMax(v); };
    auto pivot_get = [=]() { return sprite->GetPivot(); };
    auto pivot_set = [](Sprite* s, const glm::vec2& v) { s->SetPivot(v); };
    auto tex_get   = [=]() -> std::string {
        auto* t = sprite->GetTexture();
        return t ? t->GetID() : "";
    };
    auto tex_set   = [](Sprite* s, const std::string& id) { std::string v = id; s->SetTexture(v); };

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
    auto shader_set = [](Material* m, const std::string& id) { std::string v = id; m->SetShader(v); };
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
                auto set = [name = uname](Material* m, const float& v) { m->SetFloat(name, v); };
                BindProperty<float>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT));
                break;
            }
            case GL_FLOAT_VEC2: {
                auto get = [mat, name = uname]() { return mat->GetVec2(name); };
                auto set = [name = uname](Material* m, const glm::vec2& v) { m->SetVec2(name, v); };
                BindProperty<glm::vec2>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2));
                break;
            }
            case GL_FLOAT_VEC3: {
                auto get = [mat, name = uname]() { return mat->GetVec3(name); };
                auto set = [name = uname](Material* m, const glm::vec3& v) { m->SetVec3(name, v); };
                BindProperty<glm::vec3>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR3));
                break;
            }
            case GL_FLOAT_VEC4: {
                Tags widgetTag = isColorName(uname) ? Tags::COLOR : Tags::VECTOR4;
                auto get = [mat, name = uname]() { return mat->GetVec4(name); };
                auto set = [name = uname](Material* m, const glm::vec4& v) { m->SetVec4(name, v); };
                BindProperty<glm::vec4>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(widgetTag));
                break;
            }
            case GL_SAMPLER_2D: {
                auto get = [mat, name = uname]() { return mat->GetTexUniform(name); };
                auto set = [name = uname](Material* m, const std::string& id) { m->SetTexture(name, id); };
                BindProperty<std::string>(mat, uname + ": ", get, set, mat->UNIFORM_CHANGED_EVENT, PropDesc().Tag(Tags::TEXTURE).RefType(Tags::OBJECT_REF));
                break;
            }
            default:
                break;
        }
    }
}

void InspectorVisitor::Visit(Texture2D* tex) {
    // Live GL preview — spans both columns. Samples the engine's uploaded texture
    // in the shared context rather than re-decoding the file off disk, so it
    // reflects the Filtering/Wrap settings bound below.
    AddFullRow(new TexturePreviewWidget(tex->GetID()));

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

    BindProperty<std::string>(tex, "Path: ",   path_get, [](Texture2D*, const std::string&){}, Observable::CreateEvent(), PropDesc().Tag(Tags::READONLY));
    BindProperty<float>(tex,       "Width: ",  w_get,    [](Texture2D*, const float&){},       Observable::CreateEvent(), PropDesc().Tag(Tags::FLOAT).ReadOnly());
    BindProperty<float>(tex,       "Height: ", h_get,    [](Texture2D*, const float&){},       Observable::CreateEvent(), PropDesc().Tag(Tags::FLOAT).ReadOnly());

    auto filter_get = [=]() { return static_cast<int>(tex->GetFilter()); };
    auto filter_set = [](Texture2D* t, const int& v) { t->SetFilter(static_cast<TextureFilter>(v)); };
    BindProperty<int>(tex, "Filtering: ", filter_get, filter_set, tex->FILTER_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Nearest", static_cast<int>(TextureFilter::Nearest)},
            {"Linear",  static_cast<int>(TextureFilter::Linear)}
        }));

    auto wrap_get = [=]() { return static_cast<int>(tex->GetWrap()); };
    auto wrap_set = [](Texture2D* t, const int& v) { t->SetWrap(static_cast<TextureWrap>(v)); };
    BindProperty<int>(tex, "Wrap: ", wrap_get, wrap_set, tex->WRAP_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Repeat", static_cast<int>(TextureWrap::Repeat)},
            {"Clamp",  static_cast<int>(TextureWrap::Clamp)}
        }));
}

void InspectorVisitor::Visit(Shader* shader) {
    auto id_get = [=]() { return shader->GetID(); };

    BindProperty<std::string>(shader, "ID: ", id_get, [](Shader*, const std::string&){}, Observable::CreateEvent(), PropDesc().Tag(Tags::READONLY));
}

void InspectorVisitor::Visit(Camera* camera) {
    // Culling mask, viewport rect, and render-target are serialized and
    // scriptable but have no inspector widget yet -- the inspector's DROPDOWN
    // is single-select and a culling mask needs multi-select over
    // LayerManager's sorting layers, which doesn't exist as a widget today.

    auto orthoSize_get = [=]() { return camera->GetOrthoSize(); };
    auto orthoSize_set = [](Camera* c, const float& v) { c->SetOrthoSize(v); };
    BindProperty<float>(camera, "Ortho Size: ", orthoSize_get, orthoSize_set,
        camera->ORTHO_SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0.01f, 100000.0f).Step(1));

    auto priority_get = [=]() { return static_cast<float>(camera->GetPriority()); };
    auto priority_set = [](Camera* c, const float& v) { c->SetPriority(static_cast<int>(v)); };
    BindProperty<float>(camera, "Priority: ", priority_get, priority_set,
        camera->PRIORITY_CHANGED_EVENT, PropDesc().Tag(Tags::INT).Range(-32768, 32767).Step(1));

    auto aspect_get = [=]() { return camera->GetTargetAspect(); };
    auto aspect_set = [](Camera* c, const float& v) { c->SetTargetAspect(v); };
    BindProperty<float>(camera, "Target Aspect: ", aspect_get, aspect_set,
        camera->TARGET_ASPECT_CHANGED_EVENT,
        PropDesc().Tag(Tags::FLOAT).Range(0.0f, 10.0f).Step(0.01f)
            .Desc("<= 0 fills the panel (no letterboxing). e.g. 16/9 = 1.778"));

    auto clearFlags_get = [=]() { return static_cast<int>(camera->GetClearFlags()); };
    auto clearFlags_set = [](Camera* c, const int& v) { c->SetClearFlags(static_cast<RenderCamera::ClearFlags>(v)); };
    BindProperty<int>(camera, "Clear Flags: ", clearFlags_get, clearFlags_set,
        camera->CLEAR_FLAGS_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Solid Color", static_cast<int>(RenderCamera::ClearFlags::SolidColor)},
            {"Depth Only",  static_cast<int>(RenderCamera::ClearFlags::DepthOnly)},
            {"Don't Clear", static_cast<int>(RenderCamera::ClearFlags::Nothing)}
        }));

    auto clearColor_get = [=]() { return camera->GetClearColor(); };
    auto clearColor_set = [](Camera* cam, const glm::vec4& c) { cam->SetClearColor(c); };
    BindProperty<glm::vec4>(camera, "Background: ", clearColor_get, clearColor_set,
        camera->CLEAR_COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));
}

void InspectorVisitor::Visit(Animator* animator) {
    // For now the Animator inspector just shows which animation is current (the
    // authoring GUI -- setting frames, building graphs -- comes later). Read-only
    // display; it refreshes when the current animation changes.
    auto current_get = [=]() -> std::string {
        const std::string& cur = animator->GetCurrentState();
        return cur.empty() ? std::string("None") : cur;
    };
    auto current_set = [](Animator*, const std::string&) {};   // display only
    BindProperty<std::string>(animator, "Current State: ", current_get, current_set,
        animator->STATE_CHANGED_EVENT, PropDesc().Tag(Tags::READONLY));

    // Opens (creating on first use) the Animator editor panel and focuses it.
    // SyncToSelection() re-reads the current selection first, so an Animator just
    // added to this (already-selected) object shows immediately instead of the
    // stale "No Animator Selected" -- adding a component fires no selection event.
    auto* openBtn = new QPushButton("Open Animator");
    QObject::connect(openBtn, &QPushButton::clicked, openBtn, []() {
        AnimatorGui::Get()->SyncToSelection();
        MainWindow::Get()->ShowAnimator();
    });
    AddFullRow(openBtn);
}
