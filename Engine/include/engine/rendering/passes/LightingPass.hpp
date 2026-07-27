#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/core/LightBuffer.hpp"
#include "engine/rendering/core/ShadowAtlas.hpp"
#include <glm/glm.hpp>
#include <vector>

class Light;

// Gathers the scene's Light components, renders their shadow maps, and uploads
// the shared light UBO. Must run BEFORE ScenePass, which draws the sprites that
// read it.
//
// This pass draws nothing into the view's render target. It only prepares state:
// the UBO stays bound at its binding point and the shadow atlas at its reserved
// texture unit, so ScenePass needs no per-draw lighting work at all and any
// shader declaring `LightBlock` picks the data up for free.
//
// Runs once per scene (RenderPipeline loops scenePasses per scene), so lights
// illuminate their own scene only -- the same scoping ScenePass already has.
class LightingPass : public RenderPass
{
public:
    LightingPass() = default;

    void Init() override;
    void Execute(RenderCamera* camera, Scene* scene) override;
    void Shutdown() override;

private:
    // World-space AABB the camera can see, padded so a light just off-screen
    // still lights what reaches into frame.
    struct ViewBounds { glm::vec2 min{ 0.0f }; glm::vec2 max{ 0.0f }; };
    static ViewBounds ComputeViewBounds(RenderCamera* camera);

    // True if this light can affect anything inside `bounds`.
    static bool Affects(Light* light, const glm::vec2& worldPos, const ViewBounds& bounds);

    ShadowAtlas shadowAtlas;
    bool shadowsAvailable = false;

    // A light plus the world position resolved for it this frame. Resolving once
    // keeps the sort comparator free of Transform lookups (and of the null check
    // that would otherwise have to guard each one).
    struct GatheredLight
    {
        Light*    light      = nullptr;
        glm::vec2 worldPos{ 0.0f };
        float     importance = 0.0f;
    };

    // Reused across frames so a steady scene does no per-frame allocation.
    std::vector<GpuLight>      gpuLights;
    std::vector<GatheredLight> visibleLights;
    std::vector<glm::vec2>     occluderSegments;
};
