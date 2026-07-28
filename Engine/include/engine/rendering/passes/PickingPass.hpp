#pragma once
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/core/LayerManager.hpp"
#include "Engine.hpp"
#include <glad/glad.h>
#include <unordered_map>
#include <string>
#include <vector>

class GameObject;

class PickingPass : public RenderPass {
public:
    void Init() override;
    void Execute(RenderCamera* camera, Scene* scene) override; // no-op; see .cpp
    void Resize(int width, int height) override;
    void Shutdown() override;

    // Render the pick id buffer for one frame on demand (called from Pick()
    // just before ReadPixel), instead of every frame in the pipeline.
    void RenderPickBuffer(RenderCamera* camera);

    uint32_t ReadPixel(int x, int y);
    // Rect version of ReadPixel, for drag box-select: one GetPickedObjectId per
    // unique pick color found anywhere in the rect (background/id 0 excluded).
    std::vector<std::string> ReadPixelsInRect(int x, int y, int width, int height);
    std::string GetPickedObjectId(uint32_t pickId) const;
    
    void SetDebugDraw(bool enabled) { debugDraw = enabled; }
    bool GetDebugDraw() const { return debugDraw; }

private:
    void DrawDebugOverlay();

    // Picking must resolve to whatever the user SEES on top, so it replays
    // ScenePass's sort -- which is layer-priority driven, not hierarchy order.
    // Explicitly qualified: ScenePass.hpp reaches Proxy through a
    // `using namespace EngineUtils`, but doing that in a header leaks the whole
    // namespace into every TU that includes it.
    EngineUtils::Proxy<LayerManager> layerManager;

    Shader* shader = nullptr;
    GLuint debugShaderProgram = 0;
    GLuint fbo = 0;
    GLuint pickingTexture = 0;
    GLuint depthTexture = 0;
    
    // Quad for sprites (shared with ScenePass notionally, but we create our own to be safe)
    GLuint vao = 0; 
    GLuint vbo = 0;
    
    // Fullscreen quad for debug overlay
    GLuint debugVao = 0;
    GLuint debugVbo = 0;
    
    int viewportWidth = 0;
    int viewportHeight = 0;
    
    bool debugDraw = false;
    
    // Maps pick ID to GameObject ID string for lookup after ReadPixel
    std::unordered_map<uint32_t, std::string> pickIdToObjectId;
};
