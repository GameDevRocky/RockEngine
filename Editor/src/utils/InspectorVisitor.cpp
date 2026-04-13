#include "utils/InspectorVisitor.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/RigidBody.hpp"

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
    BindProperty<std::string>(renderer, "Sprite: ", sprite_get, sprite_set, renderer->MATERIAL_CHANGED_EVENT, PropDesc().Tag(Tags::SPRITE).RefType(Tags::OBJECT_REF));
    
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


void InspectorVisitor::AddRow(const std::string& text, QWidget* widget){
    QLabel* label = new QLabel(text.c_str());
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    layout->addWidget(label, gridRow, 0, Qt::AlignLeft);
    layout->addWidget(widget, gridRow, 1);
    gridRow++;
    containsContent = true;
}
