
#include "engine/rendering/RenderManager.hpp"
#include "engine/rendering/passes/DebugPass.hpp"
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/core/SceneManager.hpp"

void RenderManager::Init(){
    
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }
    glEnable(GL_DEPTH_TEST);


    editor_pipeline = new RenderPipeline();
    game_pipeline = new RenderPipeline();
    

    SetUpEditorPipeline();
}


void RenderManager::SetUpEditorPipeline(){
    auto clear_pass = new ClearPass();
    auto debug_pass = new DebugPass();

    editor_pipeline->AddPass(clear_pass);
    editor_pipeline->AddPass(debug_pass);

    editor_pipeline->Init();
}
void RenderManager::SetUpGamePipeline(){


}

void RenderManager::Update(){
   
}

void RenderManager::Render(){
    auto& scene_manager = SceneManager::Get();
    for (auto& scene : scene_manager.GetScenes()){
        editor_pipeline->Render(SceneCamera::Get(), *scene);
        
    }
}