
#include "engine/rendering/RenderManager.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"

void RenderManager::Init(){

    
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }
    glEnable(GL_DEPTH_TEST);
    
    
    editor_pipeline = new RenderPipeline();
    game_pipeline = new RenderPipeline();
    
    SharedResources::Get().Init();
    SceneCamera::Get().Init();

    SetUpEditorPipeline();
}


void RenderManager::SetUpEditorPipeline(){
    auto clear_pass = new ClearPass();
    auto grid_pass = new GridPass();

    editor_pipeline->AddPass(clear_pass);
    editor_pipeline->AddPass(grid_pass);

    editor_pipeline->Init();
}
void RenderManager::SetUpGamePipeline(){


}

void RenderManager::Update(){
    SceneCamera::Get().Update();
   
}

void RenderManager::Render(){
    auto& scene_manager = SceneManager::Get();
    for (auto& scene : scene_manager.GetScenes()){
        editor_pipeline->Render(SceneCamera::Get(), *scene);
        
    }
}