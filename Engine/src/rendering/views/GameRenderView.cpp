#include "engine/rendering/views/GameRenderView.hpp"

#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ParticleSimulationPass.hpp"
#include "engine/rendering/passes/LightingPass.hpp"
#include "engine/components/Camera.hpp"

void GameRenderView::Init()
{
    camera = new RenderCamera();
    camera->Init();

    pipeline = new RenderPipeline();

    ClearPass* clearPass = new ClearPass();
    ScenePass* scenePass = new ScenePass();

    pipeline->AddSetupPass(clearPass);
    // Simulation must precede ScenePass, which draws the particles it advanced
    // (interleaved with sprites by sorting layer).
    pipeline->AddScenePass(new ParticleSimulationPass());
    // Prepares the light UBO + shadow atlas ScenePass's sprites read from, so it
    // must precede it. Draws nothing itself.
    pipeline->AddScenePass(new LightingPass());
    pipeline->AddScenePass(scenePass);

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
