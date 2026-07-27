#include "engine/components/ShadowCaster.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <algorithm>
#include <cmath>

using namespace EngineUtils::RenderUtils;

namespace {
    constexpr float kTwoPi = 6.28318530717958647692f;

    // Push a closed polyline out as consecutive point pairs.
    void EmitClosedLoop(const std::vector<glm::vec2>& loop, std::vector<glm::vec2>& out)
    {
        const size_t n = loop.size();
        if (n < 2) return;
        for (size_t i = 0; i < n; ++i)
        {
            out.push_back(loop[i]);
            out.push_back(loop[(i + 1) % n]);
        }
    }

    void AppendBoxLoop(const glm::mat4& world, const glm::vec2& localCenter,
                       const glm::vec2& localSize, std::vector<glm::vec2>& loop)
    {
        const glm::vec2 h = localSize * 0.5f;
        const glm::vec2 corners[4] = {
            localCenter + glm::vec2(-h.x, -h.y),
            localCenter + glm::vec2( h.x, -h.y),
            localCenter + glm::vec2( h.x,  h.y),
            localCenter + glm::vec2(-h.x,  h.y),
        };
        for (const glm::vec2& c : corners)
            loop.push_back(glm::vec2(world * glm::vec4(c.x, c.y, 0.0f, 1.0f)));
    }

    void AppendCircleLoop(const glm::mat4& world, const glm::vec2& localCenter,
                          float localRadius, int segments, std::vector<glm::vec2>& loop)
    {
        segments = std::clamp(segments, 3, 64);
        for (int i = 0; i < segments; ++i)
        {
            const float a = (static_cast<float>(i) / static_cast<float>(segments)) * kTwoPi;
            const glm::vec2 p = localCenter + glm::vec2(std::cos(a), std::sin(a)) * localRadius;
            loop.push_back(glm::vec2(world * glm::vec4(p.x, p.y, 0.0f, 1.0f)));
        }
    }
}

void ShadowCaster::SetShape(Shape v)
{
    if (shape == v) return;
    shape = v;
    Notify(SHAPE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ShadowCaster::SetCenter(const glm::vec2& v)
{
    center = v;
    Notify(CENTER_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ShadowCaster::SetSize(const glm::vec2& v)
{
    size = { std::max(0.0f, v.x), std::max(0.0f, v.y) };
    Notify(SIZE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ShadowCaster::SetRadius(float v)
{
    v = std::max(0.0f, v);
    if (radius == v) return;
    radius = v;
    Notify(RADIUS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ShadowCaster::SetCircleSegments(int v)
{
    v = std::clamp(v, 3, 64);
    if (circleSegments == v) return;
    circleSegments = v;
    Notify(SEGMENTS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ShadowCaster::SetAlphaThreshold(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (alphaThreshold == v) return;
    alphaThreshold = v;
    Notify(ALPHA_THRESHOLD_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

bool ShadowCaster::ResolveSpriteQuad(Sprite*& outSprite, glm::vec2& outLocalCenter,
                                     glm::vec2& outSize)
{
    GameObject* obj = GetGameObject();
    if (!obj) return false;
    SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>();
    if (!renderer) return false;
    Sprite* sprite = renderer->GetSprite();
    if (!sprite) return false;

    const glm::vec2 worldSize = PixelsToWorld(sprite->GetPixelSize());
    if (worldSize.x <= 0.0f || worldSize.y <= 0.0f) return false;

    // Mirror sprite.glsl's vertex stage EXACTLY:
    //     scaledPos = aPos * uSize;      // aPos in [-0.5, 0.5]
    //     scaledPos -= uSize * uPivot;
    // so the quad's local centre is -uSize * pivot, with half-extent uSize/2.
    // Getting this wrong offsets the whole silhouette off the sprite -- it read
    // (pivot - 0.5) here originally, which with the project's pivot of (0,0) put
    // the shadow half a sprite up and to the right of the art casting it.
    outSprite      = sprite;
    outSize        = worldSize;
    outLocalCenter = center - worldSize * sprite->GetPivot();
    return true;
}

void ShadowCaster::BuildOutline(std::vector<glm::vec2>& out)
{
    GameObject* obj = GetGameObject();
    if (!obj) return;
    Transform* transform = obj->GetTransform();
    if (!transform) return;

    const glm::mat4 world = transform->GetWorldMatrix();

    switch (shape)
    {
        case Shape::Box:
            AppendBoxLoop(world, center, size, out);
            break;

        case Shape::Circle:
            AppendCircleLoop(world, center, radius, circleSegments, out);
            break;

        case Shape::SpriteAlpha:
            // No single closed loop exists -- a sprite silhouette can have holes
            // and disjoint islands. Callers that want to draw it use
            // AppendSegments instead.
            break;

        case Shape::SpriteBounds:
        {
            Sprite* sprite = nullptr;
            glm::vec2 quadCenter{ 0.0f }, worldSize{ 0.0f };
            if (!ResolveSpriteQuad(sprite, quadCenter, worldSize)) return;
            AppendBoxLoop(world, quadCenter, worldSize, out);
            break;
        }

        case Shape::FromCollider:
        {
            if (BoxCollider* box = obj->GetComponent<BoxCollider>())
            {
                AppendBoxLoop(world, center + box->GetCenter(), box->GetSize(), out);
            }
            else if (CircleCollider* circle = obj->GetComponent<CircleCollider>())
            {
                AppendCircleLoop(world, center + circle->GetCenter(), circle->GetRadius(),
                                 circleSegments, out);
            }
            else if (CapsuleCollider* capsule = obj->GetComponent<CapsuleCollider>())
            {
                // Approximated by its bounding box. A true capsule silhouette
                // would need two arcs joined by two edges; for a hard shadow the
                // difference is a couple of pixels at the caps and not worth the
                // extra occluder geometry.
                const float r = capsule->GetRadius();
                AppendBoxLoop(world, center + capsule->GetCenter(),
                              glm::vec2(r * 2.0f, capsule->GetHeight() + r * 2.0f), out);
            }
            break;
        }
    }
}

void ShadowCaster::AppendSegments(std::vector<glm::vec2>& out)
{
    if (shape == Shape::SpriteAlpha)
    {
        GameObject* obj = GetGameObject();
        Transform* transform = obj ? obj->GetTransform() : nullptr;
        if (!transform) return;

        Sprite* sprite = nullptr;
        glm::vec2 quadCenter{ 0.0f }, worldSize{ 0.0f };
        if (!ResolveSpriteQuad(sprite, quadCenter, worldSize)) return;

        // Cached on the Sprite and shared by every caster using it, so a tilemap
        // of identical tiles decodes the image once, not once per tile.
        const std::vector<glm::vec2>& local = sprite->GetOpaqueOutline(alphaThreshold);
        if (local.size() < 2) return;

        // The cached points are centred on the quad; quadCenter places that quad
        // in the object's local frame, and the world matrix does the rest.
        const glm::mat4 world = transform->GetWorldMatrix();
        out.reserve(out.size() + local.size());
        for (const glm::vec2& p : local)
        {
            const glm::vec2 lp = quadCenter + p;
            out.push_back(glm::vec2(world * glm::vec4(lp.x, lp.y, 0.0f, 1.0f)));
        }
        return;
    }

    std::vector<glm::vec2> loop;
    BuildOutline(loop);
    EmitClosedLoop(loop, out);
}

ShadowCaster* ShadowCaster::Copy()
{
    ShadowCaster* copy = new ShadowCaster();
    copy->id             = id;
    copy->enabled        = enabled;
    copy->gameobject_id  = gameobject_id;
    copy->shape          = shape;
    copy->center         = center;
    copy->size           = size;
    copy->radius         = radius;
    copy->circleSegments = circleSegments;
    copy->alphaThreshold = alphaThreshold;
    return copy;
}

YAML::Node ShadowCaster::Serialize()
{
    YAML::Node node = Component::Serialize();
    node["shape"] = static_cast<int>(shape);
    node["center"][0] = center.x;
    node["center"][1] = center.y;
    node["center"].SetStyle(YAML::EmitterStyle::Flow);
    node["size"][0] = size.x;
    node["size"][1] = size.y;
    node["size"].SetStyle(YAML::EmitterStyle::Flow);
    node["radius"]         = radius;
    node["circleSegments"] = circleSegments;
    node["alphaThreshold"] = alphaThreshold;
    return node;
}

void ShadowCaster::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    shape = static_cast<Shape>(node["shape"].as<int>(static_cast<int>(Shape::SpriteAlpha)));
    if (node["center"] && node["center"].IsSequence() && node["center"].size() == 2)
        center = glm::vec2(node["center"][0].as<float>(0.0f), node["center"][1].as<float>(0.0f));
    if (node["size"] && node["size"].IsSequence() && node["size"].size() == 2)
        size = glm::vec2(node["size"][0].as<float>(100.0f), node["size"][1].as<float>(100.0f));
    radius         = node["radius"].as<float>(50.0f);
    circleSegments = node["circleSegments"].as<int>(16);
    alphaThreshold = node["alphaThreshold"].as<float>(0.5f);
    state = State::Loaded;
}

void ShadowCaster::Accept(IVisitor* v)
{
    v->Visit(this);
}
