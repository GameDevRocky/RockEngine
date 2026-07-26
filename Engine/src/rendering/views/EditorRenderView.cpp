#include "engine/rendering/views/EditorRenderView.hpp"

#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ParticlePass.hpp"
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

    pipeline->AddSetupPass(clearPass);
    pipeline->AddSetupPass(gridPass);
    pipeline->AddScenePass(scenePass);
    pipeline->AddScenePass(new ParticlePass());
    pipeline->AddScenePass(debugPass);
    pipeline->AddFinalizePass(pickingPass);

    pipeline->Init();
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
