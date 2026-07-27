#include "engine/rendering/passes/LightingPass.hpp"
#include "engine/components/Light.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <algorithm>
#include <cmath>

using namespace EngineUtils;

namespace {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    // A Directional or Global light has no position, so it can never be culled;
    // sorting puts them first and they always make the cut.
    float ImportanceOf(Light* light, const glm::vec2& lightPos, const glm::vec2& viewCenter)
    {
        const auto type = light->GetType();
        if (type == Light::LightType::Directional || type == Light::LightType::Global)
            return -1.0f;
        // Nearest-to-camera wins when there are more lights than the UBO holds.
        // Distance in units of the light's own range, so a big distant light
        // outranks a tiny near one it visually dominates.
        const float d = glm::length(lightPos - viewCenter);
        return d / std::max(1.0f, light->GetRange());
    }
}

void LightingPass::Init()
{
    LightBuffer::Get().EnsureInitialized();
    shadowsAvailable = shadowAtlas.EnsureInitialized(LightLimits::kMaxShadowLights);
}

LightingPass::ViewBounds LightingPass::ComputeViewBounds(RenderCamera* camera)
{
    ViewBounds b;
    if (!camera) return b;

    const float halfHeight = camera->GetOrthoSize() / std::max(0.0001f, camera->GetZoom());
    const float aspect = static_cast<float>(std::max(1, camera->GetPixelWidth())) /
                         static_cast<float>(std::max(1, camera->GetPixelHeight()));
    const float halfWidth = halfHeight * aspect;

    // The camera may be rotated, so take the circumscribed radius rather than
    // the axis-aligned half-extents -- cheaper than transforming four corners
    // and never under-covers.
    const float radius = std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight);
    const glm::vec2 c = camera->GetPosition();
    b.min = c - glm::vec2(radius);
    b.max = c + glm::vec2(radius);
    return b;
}

bool LightingPass::Affects(Light* light, const glm::vec2& worldPos, const ViewBounds& bounds)
{
    const auto type = light->GetType();
    if (type == Light::LightType::Directional || type == Light::LightType::Global)
        return true;

    // Point/Spot: keep it if its reach overlaps the view box. A spot is treated
    // as its full circle -- narrowing to the cone would only pay off for very
    // tight cones and costs a lot more per light.
    const float r = light->GetRange();
    return worldPos.x + r >= bounds.min.x && worldPos.x - r <= bounds.max.x &&
           worldPos.y + r >= bounds.min.y && worldPos.y - r <= bounds.max.y;
}

void LightingPass::Execute(RenderCamera* camera, Scene* scene)
{
    ROCK_PROFILE_SCOPE("LightingPass");
    if (!scene) return;

    gpuLights.clear();
    visibleLights.clear();

    // Ambient starts black and accumulates every Global light. The all-white
    // fallback for a scene with NO lights at all is applied at the end.
    glm::vec4 ambient(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 ambientRGB(0.0f);
    bool sawAnyLight = false;

    const ViewBounds bounds = ComputeViewBounds(camera);
    const glm::vec2 viewCenter = camera ? camera->GetPosition() : glm::vec2(0.0f);

    // Same scan idiom as ScenePass/Camera::GetMain -- no light registry. A
    // GameObject may carry several lights, so this uses GetComponents<>.
    for (GameObject* obj : scene->GetAllGameObjects())
    {
        if (!obj || !obj->GetActive()) continue;
        Transform* transform = obj->GetTransform();
        if (!transform) continue;

        for (Light* light : obj->GetComponents<Light>())
        {
            if (!light || !light->GetEnabled()) continue;
            sawAnyLight = true;

            if (light->GetType() == Light::LightType::Global)
            {
                // Folded into the single ambient term rather than occupying a
                // UBO slot: a flat fill has nothing per-fragment to compute.
                ambientRGB += glm::vec3(light->GetColor()) * light->GetIntensity();
                continue;
            }

            const glm::vec2 worldPos = transform->GetWorldPosition();
            if (!Affects(light, worldPos, bounds)) continue;
            visibleLights.push_back({ light, worldPos, ImportanceOf(light, worldPos, viewCenter) });
        }
    }

    // Rank before the clamp so that when a scene exceeds kMaxLights the ones
    // that survive are the ones the player can actually see.
    std::sort(visibleLights.begin(), visibleLights.end(),
        [](const GatheredLight& a, const GatheredLight& b) {
            return a.importance < b.importance;
        });
    if (visibleLights.size() > static_cast<std::size_t>(LightLimits::kMaxLights))
        visibleLights.resize(LightLimits::kMaxLights);

    // ── Shadow casters ──────────────────────────────────────────────────────
    // Gathered once and reused for every shadow-casting light: the occluder set
    // is a property of the scene, not of a light.
    occluderSegments.clear();
    int shadowLightCount = 0;
    for (const GatheredLight& gathered : visibleLights)
        if (gathered.light->GetCastShadows()) ++shadowLightCount;

    // Retry resolving the shadow shader lazily. Pass Init() runs after the asset
    // load, but only a scene that actually wants shadows should pay for a retry
    // if it didn't resolve then.
    if (!shadowsAvailable && shadowLightCount > 0)
        shadowsAvailable = shadowAtlas.EnsureInitialized(LightLimits::kMaxShadowLights);

    if (shadowsAvailable && shadowLightCount > 0)
    {
        for (GameObject* obj : scene->GetAllGameObjects())
        {
            if (!obj || !obj->GetActive()) continue;
            for (ShadowCaster* caster : obj->GetComponents<ShadowCaster>())
            {
                if (!caster || !caster->GetEnabled()) continue;
                caster->AppendSegments(occluderSegments);
            }
        }
    }

    const bool renderShadows = shadowsAvailable && shadowLightCount > 0 && occluderSegments.size() >= 2;
    if (renderShadows)
    {
        shadowAtlas.BeginFrame();
        // Uploaded once for the whole frame; every light below rasterizes the
        // same buffer from its own origin.
        shadowAtlas.SetOccluders(occluderSegments);
    }

    // ── Pack ────────────────────────────────────────────────────────────────
    int nextShadowSlot = 0;
    for (const GatheredLight& gathered : visibleLights)
    {
        Light* light             = gathered.light;
        const glm::vec2 worldPos = gathered.worldPos;
        const glm::vec2 dir      = light->GetWorldDirection();
        const auto type          = light->GetType();

        GpuLight gl;
        // Directional lights carry a direction where the others carry a position;
        // the shader branches on .w so there is no ambiguity.
        gl.posRange = glm::vec4(
            type == Light::LightType::Directional ? dir : worldPos,
            light->GetRange(),
            static_cast<float>(type));
        gl.color = glm::vec4(glm::vec3(light->GetColor()), light->GetIntensity());
        gl.spot  = glm::vec4(dir,
                             std::cos(light->GetInnerAngle() * kDegToRad),
                             std::cos(light->GetOuterAngle() * kDegToRad));

        float shadowSlot = -1.0f;
        if (renderShadows && light->GetCastShadows() &&
            nextShadowSlot < LightLimits::kMaxShadowLights)
        {
            // A Directional light has no origin to cast from, so it gets no
            // shadow row -- polar shadow maps are inherently point-centric.
            if (type != Light::LightType::Directional)
            {
                shadowAtlas.RenderLight(nextShadowSlot, worldPos, light->GetRange());
                shadowSlot = static_cast<float>(nextShadowSlot);
                ++nextShadowSlot;
            }
        }

        gl.params  = glm::vec4(light->GetInnerRadius(), light->GetFalloff(),
                               shadowSlot, light->GetShadowStrength());
        gl.params2 = glm::vec4(light->GetHeight(), light->GetNormalInfluence(), 0.0f, 0.0f);

        gpuLights.push_back(gl);
    }

    if (renderShadows) shadowAtlas.EndFrame();

    // ── Upload ──────────────────────────────────────────────────────────────
    // THE backwards-compatibility case: a scene with no Light component at all
    // gets full-white ambient and zero lights, so the shader's light term is
    // exactly vec3(1.0) and sprites render as they did before lighting existed.
    // Lighting is opt-in by adding a light, never by upgrading the engine.
    if (!sawAnyLight)
        ambient = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    else
        ambient = glm::vec4(ambientRGB, 1.0f);

    LightBuffer::Get().Upload(gpuLights, ambient);

    // Bound every frame: texture unit bindings are per-context state and are not
    // shared between the Scene and Game viewports.
    if (shadowsAvailable)
        shadowAtlas.Bind(TextureSlots::ShadowAtlas);
}

void LightingPass::Shutdown()
{
    shadowAtlas.Shutdown();
    gpuLights.clear();
    visibleLights.clear();
    occluderSegments.clear();
}
