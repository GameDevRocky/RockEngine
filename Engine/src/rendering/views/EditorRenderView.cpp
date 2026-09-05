#include "engine/rendering/views/EditorRenderView.hpp"

#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ParticleSimulationPass.hpp"
#include "engine/rendering/passes/LightingPass.hpp"
#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/rendering/passes/PickingPass.hpp"

void EditorRenderView::Init()
{
    auto* editorCamera = new EditorCamera();
    camera = editorCamera;
    camera->Init();

    pipeline = new RenderPipeline();

    ClearPass* clearPass = new ClearPass();
    GridPass*  gridPass  = new GridPass();
    ScenePass* scenePass = new ScenePass();
    DebugPass* debugPass = new DebugPass();
    pickingPass = new PickingPass();

    lightingPass = new LightingPass();

    pipeline->AddSetupPass(clearPass);
    // Simulation must precede ScenePass, which draws the particles it advanced
    // (interleaved with sprites by sorting layer).
    pipeline->AddScenePass(new ParticleSimulationPass());
    // Prepares the light UBO + shadow atlas ScenePass's sprites read from, so it
    // must precede it. Draws nothing itself. Held as a member (like pickingPass)
    // so the Scene view's unshaded toggle can reach it -- this instance is the
    // editor's alone, which is what keeps the toggle out of the Game view.
    pipeline->AddScenePass(lightingPass);
    pipeline->AddScenePass(scenePass);
    pipeline->AddScenePass(debugPass);
    pipeline->AddFinalizePass(pickingPass);
    pipeline->AddFinalizePass(gridPass);

    pipeline->Init();
}

void EditorRenderView::SetLightingEnabled(bool v)
{
    if (lightingPass) lightingPass->SetLightingEnabled(v);
}

bool EditorRenderView::IsLightingEnabled() const
{
    return lightingPass ? lightingPass->IsLightingEnabled() : true;
}

std::string EditorRenderView::Pick(int fbPixelX, int fbPixelY)
{
    if (!pickingPass) return "";
    // Render the pick buffer just-in-time for this query rather than every
    // frame. UpdateCamera() first so the ids line up with what the user sees
    // (the editor camera may have panned/zoomed since the last frame render).
    UpdateCamera();
    pickingPass->RenderPickBuffer(camera);
    uint32_t pickId = pickingPass->ReadPixel(fbPixelX, fbPixelY);
    if (pickId == 0) return "";
    return pickingPass->GetPickedObjectId(pickId);
}

std::vector<std::string> EditorRenderView::PickRect(int fbPixelX, int fbPixelY, int width, int height)
{
    if (!pickingPass) return {};
    UpdateCamera();
    pickingPass->RenderPickBuffer(camera);
    return pickingPass->ReadPixelsInRect(fbPixelX, fbPixelY, width, height);
}
