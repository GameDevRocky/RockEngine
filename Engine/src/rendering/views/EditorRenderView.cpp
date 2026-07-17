#include "engine/rendering/views/EditorRenderView.hpp"

#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
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
    pipeline->AddScenePass(debugPass);
    pipeline->AddFinalizePass(pickingPass);

    pipeline->Init();
}

std::string EditorRenderView::Pick(int fbPixelX, int fbPixelY)
{
    if (!pickingPass) return "";
    uint32_t pickId = pickingPass->ReadPixel(fbPixelX, fbPixelY);
    if (pickId == 0) return "";
    return pickingPass->GetPickedObjectId(pickId);
}
