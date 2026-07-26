#include "engine/rendering/views/GameRenderView.hpp"

#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ParticlePass.hpp"
#include "engine/components/Camera.hpp"

void GameRenderView::Init()
{
    camera = new RenderCamera();
    camera->Init();

    pipeline = new RenderPipeline();

    ClearPass* clearPass = new ClearPass();
    ScenePass* scenePass = new ScenePass();

    pipeline->AddSetupPass(clearPass);
    pipeline->AddScenePass(scenePass);
    pipeline->AddScenePass(new ParticlePass());

    pipeline->Init();
}

void GameRenderView::UpdateCamera()
{
    if (Camera* mainCam = Camera::GetMain())
    {
        mainCam->ApplyTo(*camera);
        hasActiveCamera = true;
    }
    else
    {
        // No enabled Camera component in the active scenes. Leave `camera`
        // as-is rather than resetting to defaults every frame -- matches the
        // pre-Camera-component behavior of a static, unmoving view exactly.
        // The host reads HasActiveCamera() to overlay a notice.
        hasActiveCamera = false;
    }
}
