#include "engine/components/TrailRenderer.hpp"

#include "engine/components/Transform.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"

#include <algorithm>
#include <cmath>

using namespace EngineUtils;

// Resolved by NAME, not id, and tolerated if missing -- exactly as TextRenderer
// does. If Domain has not been loaded yet, or the project removed the default,
// material_id stays empty, which the draw path already reads as "nothing to
// draw" rather than as an error.
TrailRenderer::TrailRenderer() {
    if (Material* mat = AssetManager::Get().GetMaterialByName(kDefaultMaterialName))
        material_id = mat->GetID();

    // Full width at the head tapering to nothing at the tail: the shape a trail
    // is wanted for often enough that starting anywhere else is just a step
    // every user would have to undo.
    widthCurve = AnimationCurve::Linear(0.0f, 1.0f, 1.0f, 0.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Authoring
// ─────────────────────────────────────────────────────────────────────────────
void TrailRenderer::SetTime(float v) {
    // A zero or negative lifetime would expire every point on the frame it was
    // recorded, leaving a trail that never appears and no clue why.
    v = std::max(v, 0.0f);
    if (v == time) return;
    time = v;
    Notify(TIME_CHANGED_EVENT);
}

void TrailRenderer::SetMinVertexDistance(float v) {
    v = std::max(v, 0.0f);
    if (v == minVertexDistance) return;
    minVertexDistance = v;
    Notify(MIN_VERTEX_DISTANCE_CHANGED_EVENT);
}

void TrailRenderer::Clear() {
    if (points.empty()) return;
    points.clear();
    ++revision;
}

Material* TrailRenderer::GetMaterial() {
    return AssetManager::Get().GetMaterial(material_id);
}

Sprite* TrailRenderer::GetSprite() {
    if (sprite_id.empty()) return nullptr;
    return AssetManager::Get().GetSprite(sprite_id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Recording
// ─────────────────────────────────────────────────────────────────────────────
void TrailRenderer::LateUpdate() {
    Transform* transform = GetTransform();
    if (!transform) return;

    TimeManager* tm = container ? container->FindSystem<TimeManager>() : nullptr;
    if (!tm) return;

    const float now = tm->ElapsedTime();
    const glm::vec2 worldPos = transform->GetWorldPosition();

    bool changed = false;

    // Expire from the tail first, so a shrinking `time` takes effect on the same
    // frame it is edited rather than one frame later.
    while (!points.empty() && (now - points.back().birthTime) > time) {
        points.pop_back();
        changed = true;
    }

    if (emitting) {
        if (points.empty()) {
            points.push_front(TrailPoint{ worldPos, now });
            changed = true;
        } else {
            const glm::vec2 d = worldPos - points.front().pos;
            if (std::sqrt(d.x * d.x + d.y * d.y) >= minVertexDistance) {
                points.push_front(TrailPoint{ worldPos, now });
                changed = true;
            }
        }
    }

    if (changed) ++revision;

    // Runtime only. In edit mode the trail is a live preview of an authored
    // object, and a preview must never delete what it is previewing.
    if (autodestruct && !emitting && points.empty()
        && container && container->GetMode() == Container::Mode::Runtime) {
        if (GameObject* go = GetGameObject()) {
            if (Registry* reg = container->FindSystem<Registry>()) reg->Destroy(go);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw-time uniforms
// ─────────────────────────────────────────────────────────────────────────────
void TrailRenderer::OverrideUniforms(int firstFreeTextureSlot) {
    Material* mat = GetMaterial();
    if (!mat) return;
    Shader* shader = mat->GetShader();
    if (!shader) return;

    // A trail is usually a plain coloured ribbon, so an untextured one has to be
    // the cheap path: uHasTexture lets the shader skip the sample entirely
    // rather than requiring a 1x1 white texture asset to exist.
    Sprite* sprite = GetSprite();
    Texture2D* tex = sprite ? sprite->GetTexture() : nullptr;
    if (tex) {
        // Continue from where the material stopped claiming slots. Hardcoding a
        // slot here would silently clobber a material's second sampler -- the
        // bug SpriteRenderer::OverrideUniforms documents.
        const int slot = firstFreeTextureSlot;
        tex->Bind(slot);
        shader->SetTexture("uTexture", slot);
        shader->SetFloat("uHasTexture", 1.0f);

        // Sprites may be a sub-rect of an atlas, so the ribbon's [0,1] U span has
        // to be remapped into the sprite's own region or it would sample the
        // whole sheet.
        const glm::vec2 uvMin = sprite->GetUVMin();
        const glm::vec2 uvScale = sprite->GetUVMax() - uvMin;
        shader->SetVec2("uUVScale", uvScale);
        shader->SetVec2("uUVOffset", uvMin);
    } else {
        shader->SetFloat("uHasTexture", 0.0f);
        shader->SetVec2("uUVScale", glm::vec2(1.0f, 1.0f));
        shader->SetVec2("uUVOffset", glm::vec2(0.0f, 0.0f));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────
YAML::Node TrailRenderer::Serialize() {
    YAML::Node node = Component::Serialize();

    node["time"] = time;
    node["minVertexDistance"] = minVertexDistance;
    node["widthCurve"] = widthCurve.Serialize();
    node["widthMultiplier"] = widthMultiplier;

    node["startColor"][0] = startColor.r;
    node["startColor"][1] = startColor.g;
    node["startColor"][2] = startColor.b;
    node["startColor"][3] = startColor.a;
    node["startColor"].SetStyle(YAML::EmitterStyle::Flow);

    node["endColor"][0] = endColor.r;
    node["endColor"][1] = endColor.g;
    node["endColor"][2] = endColor.b;
    node["endColor"][3] = endColor.a;
    node["endColor"].SetStyle(YAML::EmitterStyle::Flow);

    node["material_id"] = material_id;
    node["sprite_id"] = sprite_id;
    node["emitting"] = emitting;
    node["autodestruct"] = autodestruct;
    node["sortingLayer"] = sortingLayer;
    node["sortingOrder"] = sortingOrder;

    // `points` and `revision` are deliberately absent -- recorded history is
    // runtime state, not authored content.
    return node;
}

void TrailRenderer::Deserialize(const YAML::Node& node) {
    Component::Deserialize(node);

    time = node["time"].as<float>(1.0f);
    minVertexDistance = node["minVertexDistance"].as<float>(2.0f);

    if (node["widthCurve"]) widthCurve.Deserialize(node["widthCurve"]);
    else                    widthCurve = AnimationCurve::Linear(0.0f, 1.0f, 1.0f, 0.0f);
    widthMultiplier = node["widthMultiplier"].as<float>(16.0f);

    if (node["startColor"])
        startColor = glm::vec4(node["startColor"][0].as<float>(), node["startColor"][1].as<float>(),
                               node["startColor"][2].as<float>(), node["startColor"][3].as<float>());
    if (node["endColor"])
        endColor = glm::vec4(node["endColor"][0].as<float>(), node["endColor"][1].as<float>(),
                             node["endColor"][2].as<float>(), node["endColor"][3].as<float>());

    material_id = node["material_id"].as<std::string>("");
    sprite_id = node["sprite_id"].as<std::string>("");
    emitting = node["emitting"].as<bool>(true);
    autodestruct = node["autodestruct"].as<bool>(false);
    sortingLayer = node["sortingLayer"].as<std::string>("Default");
    sortingOrder = node["sortingOrder"].as<int>(0);

    state = State::Loaded;
}

void TrailRenderer::Accept(IVisitor* v) {
    v->Visit(this);
}

TrailRenderer* TrailRenderer::Copy() {
    TrailRenderer* copy = new TrailRenderer();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;

    copy->time = time;
    copy->minVertexDistance = minVertexDistance;
    copy->widthCurve = widthCurve;      // value type; a plain assignment is the deep copy
    copy->widthMultiplier = widthMultiplier;

    copy->startColor = startColor;
    copy->endColor = endColor;
    copy->material_id = material_id;
    copy->sprite_id = sprite_id;

    copy->emitting = emitting;
    copy->autodestruct = autodestruct;
    copy->sortingLayer = sortingLayer;
    copy->sortingOrder = sortingOrder;

    // `points` and `revision` intentionally not copied. Recorded history is
    // transient, so pressing Play starts the trail from nothing instead of
    // inheriting whatever the edit-mode preview had drawn.
    return copy;
}
