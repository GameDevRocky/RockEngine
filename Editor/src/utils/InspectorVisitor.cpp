#include "utils/InspectorVisitor.hpp"
#include <algorithm>
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/TextRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Animator.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/Joint.hpp"
#include "engine/components/DistanceJoint.hpp"
#include "engine/components/RevoluteJoint.hpp"
#include "engine/components/PrismaticJoint.hpp"
#include "engine/components/WeldJoint.hpp"
#include "engine/components/WheelJoint.hpp"
#include "engine/components/MotorJoint.hpp"
#include "engine/components/AudioSource.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/audio/AudioClip.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/core/LayerManager.hpp"
#include "engine/core/TagManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/SelectionManager.hpp"
#include "Engine.hpp"
#include "utils/EditorUtils.hpp"
#include "utils/ComponentPickerWidget.hpp"
#include "utils/TexturePreviewWidget.hpp"
#include "dock-widgets/SpriteEditorModal.hpp"
#include "dock-widgets/MainWindowGui.hpp"
#include "dock-widgets/AnimatorGui.hpp"
#include "dock-widgets/InspectorGui.hpp"
#include <QLabel>
#include <QSizePolicy>
#include <QPushButton>
#include <map>

using namespace Properties;

InspectorVisitor::InspectorVisitor(){
    content = new QWidget();
    layout = new QGridLayout();
    layout->setContentsMargins(0,0,0,0);
    // 1/3 label, 2/3 property widget. The editors in column 1 (vec2/vec4 rows, asset
    // ref fields with their "…" button) are what actually need the room; a label only
    // needs enough to be recognised, and elides when it isn't (see ElidingLabel).
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 2);
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

void InspectorVisitor::Visit(TextRenderer* text){
    using TR = TextRenderer;

    // ── Content ──
    BindProperty<std::string>(text, "Text: ",
        [=]() { return text->GetText(); },
        [](TR* t, const std::string& v) { t->SetText(v); },
        text->TEXT_CHANGED_EVENT, PropDesc().Tag(Tags::MULTILINE));

    BindProperty<std::string>(text, "Font: ",
        [=]() { return text->GetFontID(); },
        [](TR* t, const std::string& v) { t->SetFont(v); },
        text->FONT_CHANGED_EVENT, PropDesc().Tag(Tags::FONT).RefType(Tags::OBJECT_REF));

    BindProperty<std::string>(text, "Material: ",
        [=]() { return text->GetMaterialID(); },
        [](TR* t, const std::string& v) { t->SetMaterial(v); },
        text->MATERIAL_CHANGED_EVENT, PropDesc().Tag(Tags::MATERIAL).RefType(Tags::OBJECT_REF)
            .Desc("Leave empty to use the built-in 'text' material. Any material works: a "
                  "sprite-domain shader will position the glyphs correctly but sample the "
                  "MSDF atlas as raw colour rather than decoding it."));

    // ── Layout ──
    BindProperty<float>(text, "Font Size: ",
        [=]() { return text->GetFontSize(); },
        [](TR* t, const float& v) { t->SetFontSize(v); },
        text->FONT_SIZE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0.1f, 4096).Step(1)
            .Desc("World units per em. 1 world unit == 1 pixel."));

    BindProperty<int>(text, "Horizontal Align: ",
        [=]() { return static_cast<int>(text->GetHAlign()); },
        [](TR* t, const int& v) { t->SetHAlign(static_cast<TextHAlign>(v)); },
        text->H_ALIGN_CHANGED_EVENT, PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Left",   static_cast<int>(TextHAlign::Left)},
            {"Center", static_cast<int>(TextHAlign::Center)},
            {"Right",  static_cast<int>(TextHAlign::Right)}
        }));

    BindProperty<int>(text, "Vertical Align: ",
        [=]() { return static_cast<int>(text->GetVAlign()); },
        [](TR* t, const int& v) { t->SetVAlign(static_cast<TextVAlign>(v)); },
        text->V_ALIGN_CHANGED_EVENT, PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Top",      static_cast<int>(TextVAlign::Top)},
            {"Middle",   static_cast<int>(TextVAlign::Middle)},
            {"Baseline", static_cast<int>(TextVAlign::Baseline)},
            {"Bottom",   static_cast<int>(TextVAlign::Bottom)}
        }));

    BindProperty<float>(text, "Line Spacing: ",
        [=]() { return text->GetLineSpacing(); },
        [](TR* t, const float& v) { t->SetLineSpacing(v); },
        text->LINE_SPACING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10).Step(0.05f)
            .Desc("Multiplier on the font's own line height."));

    BindProperty<float>(text, "Letter Spacing: ",
        [=]() { return text->GetLetterSpacing(); },
        [](TR* t, const float& v) { t->SetLetterSpacing(v); },
        text->LETTER_SPACING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(-1, 2).Step(0.01f)
            .Desc("Extra tracking between glyphs, in em."));

    BindProperty<float>(text, "Max Width: ",
        [=]() { return text->GetMaxWidth(); },
        [](TR* t, const float& v) { t->SetMaxWidth(v); },
        text->MAX_WIDTH_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 100000).Step(1)
            .Desc("Wrap width in world units. 0 disables wrapping."));

    // ── Appearance ──
    BindProperty<glm::vec4>(text, "Color: ",
        [=]() { return text->GetColor(); },
        [](TR* t, const glm::vec4& v) { t->SetColor(v); },
        text->COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));

    BindProperty<float>(text, "Weight: ",
        [=]() { return text->GetWeight(); },
        [](TR* t, const float& v) { t->SetWeight(v); },
        text->WEIGHT_CHANGED_EVENT, PropDesc().Tag(Tags::SLIDER).Range(-0.25f, 0.25f).Step(0.01f)
            .Desc("Shifts the distance-field threshold: negative thins the strokes, "
                  "positive bolds them. Free faux-bold, no second font file."));

    BindProperty<glm::vec4>(text, "Outline Color: ",
        [=]() { return text->GetOutlineColor(); },
        [](TR* t, const glm::vec4& v) { t->SetOutlineColor(v); },
        text->OUTLINE_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));

    BindProperty<float>(text, "Outline Width: ",
        [=]() { return text->GetOutlineWidth(); },
        [](TR* t, const float& v) { t->SetOutlineWidth(v); },
        text->OUTLINE_CHANGED_EVENT, PropDesc().Tag(Tags::SLIDER).Range(0, 0.35f).Step(0.01f)
            .Desc("0 disables the outline entirely."));

    BindProperty<bool>(text, "Visible: ",
        [=]() { return text->GetVisible(); },
        [](TR* t, const bool& v) { t->SetVisible(v); },
        text->VISIBILITY_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    // ── Sorting ── (identical semantics to SpriteRenderer, so text interleaves
    // with sprites in ScenePass's single sorted draw list.)
    LayerManager* layerManager = Engine::Get()->GetActiveContainer()->FindSystem<LayerManager>();
    if (layerManager)
    {
        std::vector<std::pair<std::string, std::any>> layerOptions;
        for (const auto& layer : layerManager->GetLayers())
            layerOptions.push_back({ layer.name, layer.priority });

        auto layer_get = [=]() -> int {
            return layerManager->GetPriority(text->GetSortingLayer());
        };
        auto layer_set = [layers = layerOptions](TR* t, const int& priority) {
            for (const auto& [name, prio] : layers)
            {
                if (std::any_cast<int>(prio) == priority)
                {
                    t->SetSortingLayer(name);
                    return;
                }
            }
        };

        BindProperty<int>(text, "Sorting Layer: ", layer_get, layer_set,
            text->SORTING_LAYER_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals(layerOptions));
    }

    BindProperty<float>(text, "Order in Layer: ",
        [=]() { return static_cast<float>(text->GetSortingOrder()); },
        [](TR* t, const float& v) { t->SetSortingOrder(static_cast<int>(v)); },
        text->SORTING_ORDER_CHANGED_EVENT,
        PropDesc().Tag(Tags::INT).Range(-32768, 32767).Step(1));
}

void InspectorVisitor::Visit(Font* font){
    using F = Font;

    BindProperty<std::string>(font, "Source: ",
        [=]() { return font->GetSourcePath(); },
        [](F*, const std::string&) {},
        font->FILE_PATH_CHANGED_EVENT, PropDesc().Tag(Tags::FILEPATH).ReadOnly());

    // Changing any of these three invalidates the disk cache and forces a rebake
    // on the next frame that draws the font.
    BindProperty<float>(font, "Atlas Em Size: ",
        [=]() { return static_cast<float>(font->GetEmSize()); },
        [](F* f, const float& v) { f->SetEmSize(static_cast<int>(v)); },
        font->BAKE_SETTINGS_CHANGED_EVENT, PropDesc().Tag(Tags::INT).Range(8, 256).Step(1)
            .Desc("Atlas pixels per em. Higher captures finer detail at a larger "
                  "texture; MSDF stays sharp when scaled up, so this rarely needs "
                  "raising for size alone."));

    BindProperty<float>(font, "Distance Range: ",
        [=]() { return font->GetPxRange(); },
        [](F* f, const float& v) { f->SetPxRange(v); },
        font->BAKE_SETTINGS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(1, 32).Step(0.5f)
            .Desc("Width of the representable distance field, in atlas pixels. "
                  "Raise it if thick outlines clip; it costs atlas area."));

    BindProperty<std::string>(font, "Charset: ",
        [=]() { return font->GetCharset(); },
        [](F* f, const std::string& v) { f->SetCharset(v); },
        font->BAKE_SETTINGS_CHANGED_EVENT, PropDesc().Tag(Tags::MULTILINE)
            .Desc("Characters to bake. Empty means printable ASCII. An icon font "
                  "needs its own glyphs listed here or it bakes empty."));

    BindProperty<std::string>(font, "Atlas: ",
        [=]() {
            if (!font->IsReady()) return std::string("not baked yet");
            return std::to_string(font->GetAtlasWidth()) + " x "
                 + std::to_string(font->GetAtlasHeight()) + " px";
        },
        [](F*, const std::string&) {},
        font->ATLAS_REBUILT_EVENT, PropDesc().Tag(Tags::STRING).ReadOnly());
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

// Fields every joint type shares. Each concrete Visit(XJoint*) below calls this
// first, so "Connected Body" always heads the section.
void InspectorVisitor::Visit(Joint* joint){
    auto getConnectedBody = [=](){ return joint->GetConnectedBody(); };
    auto setConnectedBody = [](Joint* j, const std::string& val){ j->SetConnectedBody(val); };
    auto getCollideConnected = [=](){ return joint->GetCollideConnected(); };
    auto setCollideConnected = [](Joint* j, const bool& val){ j->SetCollideConnected(val); };
    auto getAnchorA = [=](){ return joint->GetLocalAnchorA(); };
    auto setAnchorA = [](Joint* j, const glm::vec2& val){ j->SetLocalAnchorA(val); };
    auto getAnchorB = [=](){ return joint->GetLocalAnchorB(); };
    auto setAnchorB = [](Joint* j, const glm::vec2& val){ j->SetLocalAnchorB(val); };

    // ComponentType("RigidBody") restricts both the "…" picker and Hierarchy
    // drag-drop to objects that actually have a body to constrain.
    BindProperty<std::string>(joint, "Connected Body: ", getConnectedBody, setConnectedBody,
        joint->CONNECTED_BODY_CHANGED_EVENT,
        PropDesc().Tag(Tags::OBJECT_REF).RefType(Tags::OBJECT_REF).ComponentType("RigidBody"));
    BindProperty<bool>(joint, "Collide Connected: ", getCollideConnected, setCollideConnected,
        joint->COLLIDE_CONNECTED_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<glm::vec2>(joint, "Anchor A: ", getAnchorA, setAnchorA,
        joint->LOCAL_ANCHOR_A_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty<glm::vec2>(joint, "Anchor B: ", getAnchorB, setAnchorB,
        joint->LOCAL_ANCHOR_B_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
}

void InspectorVisitor::Visit(DistanceJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getLength = [=](){ return joint->GetLength(); };
    auto setLength = [](DistanceJoint* j, const float& v){ j->SetLength(v); };
    auto getEnableSpring = [=](){ return joint->GetEnableSpring(); };
    auto setEnableSpring = [](DistanceJoint* j, const bool& v){ j->SetEnableSpring(v); };
    auto getHertz = [=](){ return joint->GetHertz(); };
    auto setHertz = [](DistanceJoint* j, const float& v){ j->SetHertz(v); };
    auto getDamping = [=](){ return joint->GetDampingRatio(); };
    auto setDamping = [](DistanceJoint* j, const float& v){ j->SetDampingRatio(v); };
    auto getEnableLimit = [=](){ return joint->GetEnableLimit(); };
    auto setEnableLimit = [](DistanceJoint* j, const bool& v){ j->SetEnableLimit(v); };
    auto getMinLength = [=](){ return joint->GetMinLength(); };
    auto setMinLength = [](DistanceJoint* j, const float& v){ j->SetMinLength(v); };
    auto getMaxLength = [=](){ return joint->GetMaxLength(); };
    auto setMaxLength = [](DistanceJoint* j, const float& v){ j->SetMaxLength(v); };
    auto getEnableMotor = [=](){ return joint->GetEnableMotor(); };
    auto setEnableMotor = [](DistanceJoint* j, const bool& v){ j->SetEnableMotor(v); };
    auto getMotorSpeed = [=](){ return joint->GetMotorSpeed(); };
    auto setMotorSpeed = [](DistanceJoint* j, const float& v){ j->SetMotorSpeed(v); };
    auto getMaxMotorForce = [=](){ return joint->GetMaxMotorForce(); };
    auto setMaxMotorForce = [](DistanceJoint* j, const float& v){ j->SetMaxMotorForce(v); };

    BindProperty<float>(joint, "Length: ", getLength, setLength,
        joint->LENGTH_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
    BindProperty<bool>(joint, "Enable Spring: ", getEnableSpring, setEnableSpring,
        joint->ENABLE_SPRING_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Spring Hertz: ", getHertz, setHertz,
        joint->HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Spring Damping: ", getDamping, setDamping,
        joint->DAMPING_RATIO_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<bool>(joint, "Enable Limit: ", getEnableLimit, setEnableLimit,
        joint->ENABLE_LIMIT_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Min Length: ", getMinLength, setMinLength,
        joint->LENGTH_RANGE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
    BindProperty<float>(joint, "Max Length: ", getMaxLength, setMaxLength,
        joint->LENGTH_RANGE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX).Step(1));
    BindProperty<bool>(joint, "Enable Motor: ", getEnableMotor, setEnableMotor,
        joint->ENABLE_MOTOR_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Motor Speed: ", getMotorSpeed, setMotorSpeed,
        joint->MOTOR_SPEED_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Max Motor Force: ", getMaxMotorForce, setMaxMotorForce,
        joint->MAX_MOTOR_FORCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
}

void InspectorVisitor::Visit(RevoluteJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getTargetAngle = [=](){ return joint->GetTargetAngle(); };
    auto setTargetAngle = [](RevoluteJoint* j, const float& v){ j->SetTargetAngle(v); };
    auto getEnableSpring = [=](){ return joint->GetEnableSpring(); };
    auto setEnableSpring = [](RevoluteJoint* j, const bool& v){ j->SetEnableSpring(v); };
    auto getHertz = [=](){ return joint->GetHertz(); };
    auto setHertz = [](RevoluteJoint* j, const float& v){ j->SetHertz(v); };
    auto getDamping = [=](){ return joint->GetDampingRatio(); };
    auto setDamping = [](RevoluteJoint* j, const float& v){ j->SetDampingRatio(v); };
    auto getEnableLimit = [=](){ return joint->GetEnableLimit(); };
    auto setEnableLimit = [](RevoluteJoint* j, const bool& v){ j->SetEnableLimit(v); };
    auto getLowerAngle = [=](){ return joint->GetLowerAngle(); };
    auto setLowerAngle = [](RevoluteJoint* j, const float& v){ j->SetLowerAngle(v); };
    auto getUpperAngle = [=](){ return joint->GetUpperAngle(); };
    auto setUpperAngle = [](RevoluteJoint* j, const float& v){ j->SetUpperAngle(v); };
    auto getEnableMotor = [=](){ return joint->GetEnableMotor(); };
    auto setEnableMotor = [](RevoluteJoint* j, const bool& v){ j->SetEnableMotor(v); };
    auto getMotorSpeed = [=](){ return joint->GetMotorSpeed(); };
    auto setMotorSpeed = [](RevoluteJoint* j, const float& v){ j->SetMotorSpeed(v); };
    auto getMaxMotorTorque = [=](){ return joint->GetMaxMotorTorque(); };
    auto setMaxMotorTorque = [](RevoluteJoint* j, const float& v){ j->SetMaxMotorTorque(v); };

    BindProperty<float>(joint, "Target Angle: ", getTargetAngle, setTargetAngle,
        joint->TARGET_ANGLE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<bool>(joint, "Enable Spring: ", getEnableSpring, setEnableSpring,
        joint->ENABLE_SPRING_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Spring Hertz: ", getHertz, setHertz,
        joint->HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Spring Damping: ", getDamping, setDamping,
        joint->DAMPING_RATIO_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<bool>(joint, "Enable Limit: ", getEnableLimit, setEnableLimit,
        joint->ENABLE_LIMIT_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Lower Angle: ", getLowerAngle, setLowerAngle,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(-179, 179).Step(1));
    BindProperty<float>(joint, "Upper Angle: ", getUpperAngle, setUpperAngle,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(-179, 179).Step(1));
    BindProperty<bool>(joint, "Enable Motor: ", getEnableMotor, setEnableMotor,
        joint->ENABLE_MOTOR_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Motor Speed: ", getMotorSpeed, setMotorSpeed,
        joint->MOTOR_SPEED_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Max Motor Torque: ", getMaxMotorTorque, setMaxMotorTorque,
        joint->MAX_MOTOR_TORQUE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
}

void InspectorVisitor::Visit(PrismaticJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getAxisAngle = [=](){ return joint->GetAxisAngle(); };
    auto setAxisAngle = [](PrismaticJoint* j, const float& v){ j->SetAxisAngle(v); };
    auto getEnableSpring = [=](){ return joint->GetEnableSpring(); };
    auto setEnableSpring = [](PrismaticJoint* j, const bool& v){ j->SetEnableSpring(v); };
    auto getHertz = [=](){ return joint->GetHertz(); };
    auto setHertz = [](PrismaticJoint* j, const float& v){ j->SetHertz(v); };
    auto getDamping = [=](){ return joint->GetDampingRatio(); };
    auto setDamping = [](PrismaticJoint* j, const float& v){ j->SetDampingRatio(v); };
    auto getTargetTranslation = [=](){ return joint->GetTargetTranslation(); };
    auto setTargetTranslation = [](PrismaticJoint* j, const float& v){ j->SetTargetTranslation(v); };
    auto getEnableLimit = [=](){ return joint->GetEnableLimit(); };
    auto setEnableLimit = [](PrismaticJoint* j, const bool& v){ j->SetEnableLimit(v); };
    auto getLower = [=](){ return joint->GetLowerTranslation(); };
    auto setLower = [](PrismaticJoint* j, const float& v){ j->SetLowerTranslation(v); };
    auto getUpper = [=](){ return joint->GetUpperTranslation(); };
    auto setUpper = [](PrismaticJoint* j, const float& v){ j->SetUpperTranslation(v); };
    auto getEnableMotor = [=](){ return joint->GetEnableMotor(); };
    auto setEnableMotor = [](PrismaticJoint* j, const bool& v){ j->SetEnableMotor(v); };
    auto getMotorSpeed = [=](){ return joint->GetMotorSpeed(); };
    auto setMotorSpeed = [](PrismaticJoint* j, const float& v){ j->SetMotorSpeed(v); };
    auto getMaxMotorForce = [=](){ return joint->GetMaxMotorForce(); };
    auto setMaxMotorForce = [](PrismaticJoint* j, const float& v){ j->SetMaxMotorForce(v); };

    BindProperty<float>(joint, "Axis Angle: ", getAxisAngle, setAxisAngle,
        joint->AXIS_ANGLE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(-360, 360).Step(1));
    BindProperty<bool>(joint, "Enable Spring: ", getEnableSpring, setEnableSpring,
        joint->ENABLE_SPRING_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Spring Hertz: ", getHertz, setHertz,
        joint->HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Spring Damping: ", getDamping, setDamping,
        joint->DAMPING_RATIO_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<float>(joint, "Target Translation: ", getTargetTranslation, setTargetTranslation,
        joint->TARGET_TRANSLATION_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<bool>(joint, "Enable Limit: ", getEnableLimit, setEnableLimit,
        joint->ENABLE_LIMIT_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Lower Translation: ", getLower, setLower,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Upper Translation: ", getUpper, setUpper,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<bool>(joint, "Enable Motor: ", getEnableMotor, setEnableMotor,
        joint->ENABLE_MOTOR_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Motor Speed: ", getMotorSpeed, setMotorSpeed,
        joint->MOTOR_SPEED_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Max Motor Force: ", getMaxMotorForce, setMaxMotorForce,
        joint->MAX_MOTOR_FORCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
}

void InspectorVisitor::Visit(WeldJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getLinearHertz = [=](){ return joint->GetLinearHertz(); };
    auto setLinearHertz = [](WeldJoint* j, const float& v){ j->SetLinearHertz(v); };
    auto getLinearDamping = [=](){ return joint->GetLinearDampingRatio(); };
    auto setLinearDamping = [](WeldJoint* j, const float& v){ j->SetLinearDampingRatio(v); };
    auto getAngularHertz = [=](){ return joint->GetAngularHertz(); };
    auto setAngularHertz = [](WeldJoint* j, const float& v){ j->SetAngularHertz(v); };
    auto getAngularDamping = [=](){ return joint->GetAngularDampingRatio(); };
    auto setAngularDamping = [](WeldJoint* j, const float& v){ j->SetAngularDampingRatio(v); };

    // 0 Hertz means "infinitely stiff" here, which is the rigid-weld default.
    BindProperty<float>(joint, "Linear Hertz: ", getLinearHertz, setLinearHertz,
        joint->LINEAR_HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Linear Damping: ", getLinearDamping, setLinearDamping,
        joint->LINEAR_DAMPING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<float>(joint, "Angular Hertz: ", getAngularHertz, setAngularHertz,
        joint->ANGULAR_HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Angular Damping: ", getAngularDamping, setAngularDamping,
        joint->ANGULAR_DAMPING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
}

void InspectorVisitor::Visit(WheelJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getAxisAngle = [=](){ return joint->GetAxisAngle(); };
    auto setAxisAngle = [](WheelJoint* j, const float& v){ j->SetAxisAngle(v); };
    auto getEnableSpring = [=](){ return joint->GetEnableSpring(); };
    auto setEnableSpring = [](WheelJoint* j, const bool& v){ j->SetEnableSpring(v); };
    auto getHertz = [=](){ return joint->GetHertz(); };
    auto setHertz = [](WheelJoint* j, const float& v){ j->SetHertz(v); };
    auto getDamping = [=](){ return joint->GetDampingRatio(); };
    auto setDamping = [](WheelJoint* j, const float& v){ j->SetDampingRatio(v); };
    auto getEnableLimit = [=](){ return joint->GetEnableLimit(); };
    auto setEnableLimit = [](WheelJoint* j, const bool& v){ j->SetEnableLimit(v); };
    auto getLower = [=](){ return joint->GetLowerTranslation(); };
    auto setLower = [](WheelJoint* j, const float& v){ j->SetLowerTranslation(v); };
    auto getUpper = [=](){ return joint->GetUpperTranslation(); };
    auto setUpper = [](WheelJoint* j, const float& v){ j->SetUpperTranslation(v); };
    auto getEnableMotor = [=](){ return joint->GetEnableMotor(); };
    auto setEnableMotor = [](WheelJoint* j, const bool& v){ j->SetEnableMotor(v); };
    auto getMotorSpeed = [=](){ return joint->GetMotorSpeed(); };
    auto setMotorSpeed = [](WheelJoint* j, const float& v){ j->SetMotorSpeed(v); };
    auto getMaxMotorTorque = [=](){ return joint->GetMaxMotorTorque(); };
    auto setMaxMotorTorque = [](WheelJoint* j, const float& v){ j->SetMaxMotorTorque(v); };

    BindProperty<float>(joint, "Suspension Angle: ", getAxisAngle, setAxisAngle,
        joint->AXIS_ANGLE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(-360, 360).Step(1));
    BindProperty<bool>(joint, "Enable Spring: ", getEnableSpring, setEnableSpring,
        joint->ENABLE_SPRING_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Spring Hertz: ", getHertz, setHertz,
        joint->HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Spring Damping: ", getDamping, setDamping,
        joint->DAMPING_RATIO_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<bool>(joint, "Enable Limit: ", getEnableLimit, setEnableLimit,
        joint->ENABLE_LIMIT_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Lower Translation: ", getLower, setLower,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Upper Translation: ", getUpper, setUpper,
        joint->LIMITS_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<bool>(joint, "Enable Motor: ", getEnableMotor, setEnableMotor,
        joint->ENABLE_MOTOR_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));
    BindProperty<float>(joint, "Motor Speed: ", getMotorSpeed, setMotorSpeed,
        joint->MOTOR_SPEED_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Max Motor Torque: ", getMaxMotorTorque, setMaxMotorTorque,
        joint->MAX_MOTOR_TORQUE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
}

void InspectorVisitor::Visit(MotorJoint* joint){
    Visit(static_cast<Joint*>(joint));

    auto getLinearVelocity = [=](){ return joint->GetLinearVelocity(); };
    auto setLinearVelocity = [](MotorJoint* j, const glm::vec2& v){ j->SetLinearVelocity(v); };
    auto getMaxVelocityForce = [=](){ return joint->GetMaxVelocityForce(); };
    auto setMaxVelocityForce = [](MotorJoint* j, const float& v){ j->SetMaxVelocityForce(v); };
    auto getAngularVelocity = [=](){ return joint->GetAngularVelocity(); };
    auto setAngularVelocity = [](MotorJoint* j, const float& v){ j->SetAngularVelocity(v); };
    auto getMaxVelocityTorque = [=](){ return joint->GetMaxVelocityTorque(); };
    auto setMaxVelocityTorque = [](MotorJoint* j, const float& v){ j->SetMaxVelocityTorque(v); };
    auto getLinearHertz = [=](){ return joint->GetLinearHertz(); };
    auto setLinearHertz = [](MotorJoint* j, const float& v){ j->SetLinearHertz(v); };
    auto getLinearDamping = [=](){ return joint->GetLinearDampingRatio(); };
    auto setLinearDamping = [](MotorJoint* j, const float& v){ j->SetLinearDampingRatio(v); };
    auto getMaxSpringForce = [=](){ return joint->GetMaxSpringForce(); };
    auto setMaxSpringForce = [](MotorJoint* j, const float& v){ j->SetMaxSpringForce(v); };
    auto getAngularHertz = [=](){ return joint->GetAngularHertz(); };
    auto setAngularHertz = [](MotorJoint* j, const float& v){ j->SetAngularHertz(v); };
    auto getAngularDamping = [=](){ return joint->GetAngularDampingRatio(); };
    auto setAngularDamping = [](MotorJoint* j, const float& v){ j->SetAngularDampingRatio(v); };
    auto getMaxSpringTorque = [=](){ return joint->GetMaxSpringTorque(); };
    auto setMaxSpringTorque = [](MotorJoint* j, const float& v){ j->SetMaxSpringTorque(v); };

    BindProperty<glm::vec2>(joint, "Linear Velocity: ", getLinearVelocity, setLinearVelocity,
        joint->LINEAR_VELOCITY_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1));
    BindProperty<float>(joint, "Max Velocity Force: ", getMaxVelocityForce, setMaxVelocityForce,
        joint->MAX_VELOCITY_FORCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Angular Velocity: ", getAngularVelocity, setAngularVelocity,
        joint->ANGULAR_VELOCITY_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Step(1));
    BindProperty<float>(joint, "Max Velocity Torque: ", getMaxVelocityTorque, setMaxVelocityTorque,
        joint->MAX_VELOCITY_TORQUE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Linear Hertz: ", getLinearHertz, setLinearHertz,
        joint->LINEAR_HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Linear Damping: ", getLinearDamping, setLinearDamping,
        joint->LINEAR_DAMPING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<float>(joint, "Max Spring Force: ", getMaxSpringForce, setMaxSpringForce,
        joint->MAX_SPRING_FORCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Angular Hertz: ", getAngularHertz, setAngularHertz,
        joint->ANGULAR_HERTZ_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
    BindProperty<float>(joint, "Angular Damping: ", getAngularDamping, setAngularDamping,
        joint->ANGULAR_DAMPING_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, 10));
    BindProperty<float>(joint, "Max Spring Torque: ", getMaxSpringTorque, setMaxSpringTorque,
        joint->MAX_SPRING_TORQUE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0, INT_MAX));
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

    BindProperty<bool>(p, "Flip X: ",
        [=]() { return p->GetFlipX(); },
        [](PC* e, const bool& v) { e->SetFlipX(v); },
        p->FLIP_X_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    BindProperty<bool>(p, "Flip Y: ",
        [=]() { return p->GetFlipY(); },
        [](PC* e, const bool& v) { e->SetFlipY(v); },
        p->FLIP_Y_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

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

    // Same layer model as SpriteRenderer -- emitters sort against sprites in one
    // list (see ScenePass), so this places the effect among the sprite layers.
    if (LayerManager* particleLayers = Engine::Get()->GetActiveContainer()->FindSystem<LayerManager>())
    {
        std::vector<std::pair<std::string, std::any>> layerOptions;
        for (const auto& layer : particleLayers->GetLayers())
            layerOptions.push_back({ layer.name, layer.priority });

        auto layer_get = [=]() -> int { return particleLayers->GetPriority(p->GetSortingLayer()); };
        // Captures the priority->name pairs by value so the setter is pointer-free.
        auto layer_set = [layers = layerOptions](PC* e, const int& priority) {
            for (const auto& [name, prio] : layers)
            {
                if (std::any_cast<int>(prio) == priority)
                {
                    e->SetSortingLayer(name);
                    return;
                }
            }
        };

        BindProperty<int>(p, "Sorting Layer: ", layer_get, layer_set,
            p->SORTING_LAYER_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals(layerOptions));
    }

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

void InspectorVisitor::Visit(Light* light) {
    using LT = Light::LightType;
    const LT type = light->GetType();

    BindProperty<int>(light, "Type: ",
        [=]() { return static_cast<int>(light->GetType()); },
        [](Light* l, const int& v) { l->SetType(static_cast<LT>(v)); },
        light->TYPE_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Point",       static_cast<int>(LT::Point)},
            {"Spot",        static_cast<int>(LT::Spot)},
            {"Directional", static_cast<int>(LT::Directional)},
            {"Global",      static_cast<int>(LT::Global)}
        }));

    // Which rows below are meaningful depends on Type, so a type change has to
    // rebuild the whole section rather than just refresh a value. Subscribed
    // here (not inside BindProperty) because this is a structural response, not
    // a value response.
    {
        int subId = light->Subscribe([]() {
            InspectorGui::Get()->RequestRebuild();
            return true;
        }, light->TYPE_CHANGED_EVENT);
        m_subscriptions.emplace_back(light, subId);
    }

    BindProperty<glm::vec4>(light, "Color: ",
        [=]() { return light->GetColor(); },
        [](Light* l, const glm::vec4& v) { l->SetColor(v); },
        light->COLOR_CHANGED_EVENT, PropDesc().Tag(Tags::COLOR));

    BindProperty<float>(light, "Intensity: ",
        [=]() { return light->GetIntensity(); },
        [](Light* l, const float& v) { l->SetIntensity(v); },
        light->INTENSITY_CHANGED_EVENT,
        PropDesc().Tag(Tags::FLOAT).Range(0.0f, 100.0f).Step(0.05f));

    // ── Attenuation: Point and Spot only ────────────────────────────────────
    if (type == LT::Point || type == LT::Spot) {
        BindProperty<float>(light, "Range: ",
            [=]() { return light->GetRange(); },
            [](Light* l, const float& v) { l->SetRange(v); },
            light->RANGE_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.001f, 100000.0f).Step(5.0f)
                .Desc("Reach in world units (world units == pixels)."));

        BindProperty<float>(light, "Inner Radius: ",
            [=]() { return light->GetInnerRadius(); },
            [](Light* l, const float& v) { l->SetInnerRadius(v); },
            light->INNER_RADIUS_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 0.99f).Step(0.01f)
                .Desc("Fraction of Range held at full brightness before falloff starts."));

        BindProperty<float>(light, "Falloff: ",
            [=]() { return light->GetFalloff(); },
            [](Light* l, const float& v) { l->SetFalloff(v); },
            light->FALLOFF_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.01f, 16.0f).Step(0.1f)
                .Desc("Attenuation exponent. 1 = soft ramp, higher = tighter hotspot."));
    }

    // ── Cone: Spot only ─────────────────────────────────────────────────────
    if (type == LT::Spot) {
        BindProperty<float>(light, "Inner Angle: ",
            [=]() { return light->GetInnerAngle(); },
            [](Light* l, const float& v) { l->SetInnerAngle(v); },
            light->INNER_ANGLE_CHANGED_EVENT,
            PropDesc().Tag(Tags::ANGLE).Range(0.0f, 180.0f).Step(1.0f)
                .Desc("Half-angle of the full-strength core, in degrees."));

        BindProperty<float>(light, "Outer Angle: ",
            [=]() { return light->GetOuterAngle(); },
            [](Light* l, const float& v) { l->SetOuterAngle(v); },
            light->OUTER_ANGLE_CHANGED_EVENT,
            PropDesc().Tag(Tags::ANGLE).Range(0.0f, 180.0f).Step(1.0f)
                .Desc("Half-angle where the cone reaches zero. Direction comes from the Transform's rotation."));
    }

    // ── Normal-map response: everything except the flat ambient fill ────────
    if (type != LT::Global) {
        BindProperty<float>(light, "Height: ",
            [=]() { return light->GetHeight(); },
            [](Light* l, const float& v) { l->SetHeight(v); },
            light->HEIGHT_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 10000.0f).Step(5.0f)
                .Desc("Distance above the sprite plane. Sprites are all at z=0, so a "
                      "height of 0 makes normal maps produce no shading at all."));

        BindProperty<float>(light, "Normal Influence: ",
            [=]() { return light->GetNormalInfluence(); },
            [](Light* l, const float& v) { l->SetNormalInfluence(v); },
            light->NORMAL_INFLUENCE_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 1.0f).Step(0.05f)
                .Desc("0 = flat lighting, 1 = full response to the surface normal."));

    }

    // ── Shadows: Point and Spot only ────────────────────────────────────────
    // A Directional light has no origin to cast from, and the shadow atlas is a
    // POLAR depth map around a point -- there is nothing for it to rasterize.
    // LightingPass skips Directional when handing out atlas rows, so offering
    // the toggle here would be a control that silently does nothing.
    if (type == LT::Point || type == LT::Spot) {
        BindProperty<bool>(light, "Cast Shadows: ",
            [=]() { return light->GetCastShadows(); },
            [](Light* l, const bool& v) { l->SetCastShadows(v); },
            light->CAST_SHADOWS_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE)
                .Desc("Requires a ShadowCaster component on the objects that should block this light."));

        if (light->GetCastShadows()) {
            BindProperty<float>(light, "Shadow Strength: ",
                [=]() { return light->GetShadowStrength(); },
                [](Light* l, const float& v) { l->SetShadowStrength(v); },
                light->SHADOW_STRENGTH_CHANGED_EVENT,
                PropDesc().Tag(Tags::FLOAT).Range(0.0f, 1.0f).Step(0.05f)
                    .Desc("0 = shadow does nothing, 1 = fully occluded."));
        }

        // Same structural-rebuild reasoning as Type: toggling shadows on adds
        // the strength row.
        int subId = light->Subscribe([]() {
            InspectorGui::Get()->RequestRebuild();
            return true;
        }, light->CAST_SHADOWS_CHANGED_EVENT);
        m_subscriptions.emplace_back(light, subId);
    }
}

void InspectorVisitor::Visit(ShadowCaster* caster) {
    using CS = ShadowCaster::Shape;
    const CS shape = caster->GetShape();

    BindProperty<int>(caster, "Shape: ",
        [=]() { return static_cast<int>(caster->GetShape()); },
        [](ShadowCaster* c, const int& v) { c->SetShape(static_cast<CS>(v)); },
        caster->SHAPE_CHANGED_EVENT,
        PropDesc().Tag(Tags::DROPDOWN).DropVals({
            {"Sprite Alpha",  static_cast<int>(CS::SpriteAlpha)},
            {"Sprite Bounds", static_cast<int>(CS::SpriteBounds)},
            {"Box",           static_cast<int>(CS::Box)},
            {"Circle",        static_cast<int>(CS::Circle)},
            {"From Collider", static_cast<int>(CS::FromCollider)}
        }).Desc("Sprite Alpha traces the opaque pixels so light passes through "
                "transparent ones. Sprite Bounds is the plain rect -- cheaper, and "
                "identical for fully-opaque art."));

    {
        int subId = caster->Subscribe([]() {
            InspectorGui::Get()->RequestRebuild();
            return true;
        }, caster->SHAPE_CHANGED_EVENT);
        m_subscriptions.emplace_back(caster, subId);
    }

    BindProperty<glm::vec2>(caster, "Center: ",
        [=]() { return caster->GetCenter(); },
        [](ShadowCaster* c, const glm::vec2& v) { c->SetCenter(v); },
        caster->CENTER_CHANGED_EVENT, PropDesc().Tag(Tags::VECTOR2).Step(1.0f));

    if (shape == CS::SpriteAlpha) {
        BindProperty<float>(caster, "Alpha Threshold: ",
            [=]() { return caster->GetAlphaThreshold(); },
            [](ShadowCaster* c, const float& v) { c->SetAlphaThreshold(v); },
            caster->ALPHA_THRESHOLD_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 1.0f).Step(0.05f)
                .Desc("Pixels at or above this alpha block light. Raise it to stop "
                      "soft anti-aliased edges from casting."));
    }

    if (shape == CS::Box) {
        BindProperty<glm::vec2>(caster, "Size: ",
            [=]() { return caster->GetSize(); },
            [](ShadowCaster* c, const glm::vec2& v) { c->SetSize(v); },
            caster->SIZE_CHANGED_EVENT,
            PropDesc().Tag(Tags::VECTOR2).Range(0.0f, 100000.0f).Step(1.0f));
    }

    if (shape == CS::Circle) {
        BindProperty<float>(caster, "Radius: ",
            [=]() { return caster->GetRadius(); },
            [](ShadowCaster* c, const float& v) { c->SetRadius(v); },
            caster->RADIUS_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 100000.0f).Step(1.0f));
    }

    if (shape == CS::Circle || shape == CS::FromCollider) {
        BindProperty<float>(caster, "Circle Segments: ",
            [=]() { return static_cast<float>(caster->GetCircleSegments()); },
            [](ShadowCaster* c, const float& v) { c->SetCircleSegments(static_cast<int>(v)); },
            caster->SEGMENTS_CHANGED_EVENT,
            PropDesc().Tag(Tags::INT).Range(3, 64).Step(1)
                .Desc("Edges used to approximate a round silhouette."));
    }
}

void InspectorVisitor::AddRow(const std::string& text, QWidget* widget){
    // Every property row's label is normalised here, so a call site can pass a
    // hand-written "Color: " or a raw field name like "startOffset" and both come out
    // as "Start Offset: ". The ": " is appended once here rather than repeated at every
    // call site -- which is exactly why FormatLabel strips a trailing one first.
    auto* label = new EditorUtils::ElidingLabel(
        QString::fromStdString(EditorUtils::FormatLabel(text) + ": "));
    auto font = label->font();
    font.setBold(true);
    label->setFont(font);

    // Tall widgets (the multi-line text box) ask for their label on the first
    // line rather than centred against the whole block — see TextBoxPropertyWidget.
    //
    // Applied as the label's OWN text alignment instead of a cell alignment handed to
    // addWidget, which is not a cosmetic difference: a cell alignment makes the layout
    // shrink the widget to its sizeHint, so the label would size itself to its full
    // text and never be narrow enough to elide. Unaligned, it fills the column and
    // ElidingLabel fits the text to that width.
    label->setAlignment(widget->property("labelTopAlign").toBool()
                        ? (Qt::AlignLeft | Qt::AlignTop)
                        : (Qt::AlignLeft | Qt::AlignVCenter));

    layout->addWidget(label, gridRow, 0);
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

    // ── Apply Normal ────────────────────────────────────────────────────────
    // Derives a tangent-space normal map from this texture's own grayscale so
    // lights shade its surface detail. The generated image is IN MEMORY ONLY --
    // only the settings below are written (into the .texture meta), and the map
    // is rebuilt from them on load. Every field regenerates it on the next draw.
    BindProperty<bool>(tex, "Apply Normal: ",
        [=]() { return tex->GetApplyNormal(); },
        [](Texture2D* t, const bool& v) { t->SetApplyNormal(v); },
        tex->NORMAL_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE)
            .Desc("Generate a normal map from this texture's grayscale. Never written to disk."));

    // Toggling reveals/hides the tuning fields, which is a structural change.
    {
        int subId = tex->Subscribe([]() {
            InspectorGui::Get()->RequestRebuild();
            return true;
        }, tex->NORMAL_CHANGED_EVENT);
        m_subscriptions.emplace_back(tex, subId);
    }

    if (tex->GetApplyNormal()) {
        BindProperty<int>(tex, "Height Source: ",
            [=]() { return static_cast<int>(tex->GetNormalHeightSource()); },
            [](Texture2D* t, const int& v) { t->SetNormalHeightSource(static_cast<NormalHeightSource>(v)); },
            tex->NORMAL_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals({
                {"Luminance", static_cast<int>(NormalHeightSource::Luminance)},
                {"Alpha",     static_cast<int>(NormalHeightSource::Alpha)}
            }).Desc("Which channel is read as height. Alpha suits flat-coloured sprites."));

        BindProperty<int>(tex, "Edge Filter: ",
            [=]() { return static_cast<int>(tex->GetNormalEdgeFilter()); },
            [](Texture2D* t, const int& v) { t->SetNormalEdgeFilter(static_cast<NormalEdgeFilter>(v)); },
            tex->NORMAL_CHANGED_EVENT,
            PropDesc().Tag(Tags::DROPDOWN).DropVals({
                {"Sobel",  static_cast<int>(NormalEdgeFilter::Sobel)},
                {"Scharr", static_cast<int>(NormalEdgeFilter::Scharr)}
            }).Desc("Gradient operator. Scharr gives smoother diagonals, Sobel is crisper."));

        BindProperty<float>(tex, "Strength: ",
            [=]() { return tex->GetNormalStrength(); },
            [](Texture2D* t, const float& v) { t->SetNormalStrength(v); },
            tex->NORMAL_CHANGED_EVENT,
            PropDesc().Tag(Tags::FLOAT).Range(0.0f, 100.0f).Step(0.1f)
                .Desc("Slope scale. Higher = deeper apparent relief."));

        BindProperty<float>(tex, "Blur: ",
            [=]() { return static_cast<float>(tex->GetNormalBlur()); },
            [](Texture2D* t, const float& v) { t->SetNormalBlur(static_cast<int>(v)); },
            tex->NORMAL_CHANGED_EVENT,
            PropDesc().Tag(Tags::INT).Range(0, 8).Step(1)
                .Desc("Box-blur radius on the height field. Smooths pixel noise into soft relief."));

        BindProperty<bool>(tex, "Invert X: ",
            [=]() { return tex->GetNormalInvertX(); },
            [](Texture2D* t, const bool& v) { t->SetNormalInvertX(v); },
            tex->NORMAL_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

        BindProperty<bool>(tex, "Invert Y: ",
            [=]() { return tex->GetNormalInvertY(); },
            [](Texture2D* t, const bool& v) { t->SetNormalInvertY(v); },
            tex->NORMAL_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

        AddFullRow(new TexturePreviewWidget(tex->GetID(), TexturePreviewWidget::Source::NormalMap));
    }
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

void InspectorVisitor::Visit(AudioSource* src) {
    using AS = AudioSource;

    auto clip_get = [=]() { return src->GetClipID(); };
    auto clip_set = [](AS* s, const std::string& val) { std::string v = val; s->SetClip(v); };
    BindProperty<std::string>(src, "Clip: ", clip_get, clip_set,
        src->CLIP_CHANGED_EVENT, PropDesc().Tag(Tags::AUDIO_CLIP).RefType(Tags::OBJECT_REF));

    BindProperty<bool>(src, "Play On Awake: ",
        [=]() { return src->GetPlayOnAwake(); },
        [](AS* s, const bool& v) { s->SetPlayOnAwake(v); },
        src->PLAY_ON_AWAKE_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    BindProperty<bool>(src, "Loop: ",
        [=]() { return src->GetLoop(); },
        [](AS* s, const bool& v) { s->SetLoop(v); },
        src->LOOP_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    BindProperty<bool>(src, "Mute: ",
        [=]() { return src->GetMute(); },
        [](AS* s, const bool& v) { s->SetMute(v); },
        src->MUTE_CHANGED_EVENT, PropDesc().Tag(Tags::TOGGLE));

    BindProperty<float>(src, "Volume: ",
        [=]() { return src->GetVolume(); },
        [](AS* s, const float& v) { s->SetVolume(v); },
        src->VOLUME_CHANGED_EVENT, PropDesc().Tag(Tags::SLIDER).Range(0.0f, 1.0f).Step(0.01f));

    BindProperty<float>(src, "Pitch: ",
        [=]() { return src->GetPitch(); },
        [](AS* s, const float& v) { s->SetPitch(v); },
        src->PITCH_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0.1f, 3.0f).Step(0.01f));

    BindProperty<float>(src, "Spatial Blend: ",
        [=]() { return src->GetSpatialBlend(); },
        [](AS* s, const float& v) { s->SetSpatialBlend(v); },
        src->SPATIAL_BLEND_CHANGED_EVENT, PropDesc().Tag(Tags::SLIDER).Range(0.0f, 1.0f).Step(0.01f)
            .Desc("0 = always centered, full volume regardless of distance (music/UI). "
                  "1 = fully positional, panned and attenuated against the active "
                  "AudioListener (falls back to the main Camera if none is present)."));

    BindProperty<float>(src, "Min Distance: ",
        [=]() { return src->GetMinDistance(); },
        [](AS* s, const float& v) { s->SetMinDistance(v); },
        src->MIN_DISTANCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0.0f, 100000.0f).Step(1.0f)
            .Desc("World units (== pixels). Distance under which volume stays at max."));

    BindProperty<float>(src, "Max Distance: ",
        [=]() { return src->GetMaxDistance(); },
        [](AS* s, const float& v) { s->SetMaxDistance(v); },
        src->MAX_DISTANCE_CHANGED_EVENT, PropDesc().Tag(Tags::FLOAT).Range(0.0f, 1000000.0f).Step(1.0f)
            .Desc("World units (== pixels). Distance beyond which volume is silent."));

    auto* playBtn = new QPushButton("Play");
    QObject::connect(playBtn, &QPushButton::clicked, playBtn, [src]() { src->Play(); });
    AddFullRow(playBtn);

    auto* stopBtn = new QPushButton("Stop");
    QObject::connect(stopBtn, &QPushButton::clicked, stopBtn, [src]() { src->Stop(); });
    AddFullRow(stopBtn);
}

void InspectorVisitor::Visit(AudioListener*) {
    auto* label = new QLabel(
        "Drives the audio listener from this GameObject's position. "
        "At most one active AudioListener should exist per scene.");
    label->setWordWrap(true);
    AddFullRow(label);
}

void InspectorVisitor::Visit(AudioClip* clip) {
    BindProperty<std::string>(clip, "Path: ",
        [=]() { return clip->GetPath(); },
        [](AudioClip*, const std::string&) {},
        clip->FILE_PATH_CHANGED_EVENT, PropDesc().Tag(Tags::FILEPATH).ReadOnly());

    BindProperty<std::string>(clip, "Duration: ",
        [=]() {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f s", clip->GetDuration());
            return std::string(buf);
        },
        [](AudioClip*, const std::string&) {},
        clip->FILE_PATH_CHANGED_EVENT, PropDesc().Tag(Tags::STRING).ReadOnly());

    BindProperty<std::string>(clip, "Channels: ",
        [=]() { return std::to_string(clip->GetChannels()); },
        [](AudioClip*, const std::string&) {},
        clip->FILE_PATH_CHANGED_EVENT, PropDesc().Tag(Tags::STRING).ReadOnly());

    BindProperty<std::string>(clip, "Sample Rate: ",
        [=]() { return std::to_string(clip->GetSampleRate()) + " Hz"; },
        [](AudioClip*, const std::string&) {},
        clip->FILE_PATH_CHANGED_EVENT, PropDesc().Tag(Tags::STRING).ReadOnly());

    auto* playBtn = new QPushButton("Play Preview");
    QObject::connect(playBtn, &QPushButton::clicked, playBtn, [clip]() {
        AudioEngine::Get().PlayOneShot(clip->GetPath());
    });
    AddFullRow(playBtn);
}
