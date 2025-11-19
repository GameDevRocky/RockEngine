
#include "engine/rendering/RenderManager.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"

void RenderManager::Init(){

    
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }
    glEnable(GL_DEPTH_TEST);
    Console::Comment("RenderManager Initialized");
    SharedResources::Get().Init();
    
    
    editor_pipeline = new RenderPipeline();
    game_pipeline = new RenderPipeline();
    
    SceneCamera::Get().Init();

    SetUpEditorPipeline();
}


void RenderManager::SetUpEditorPipeline(){
    auto clear_pass = new ClearPass();
    auto scene_pass = new ScenePass();

    editor_pipeline->AddPass(clear_pass);
    editor_pipeline->AddPass(scene_pass);

    editor_pipeline->Init();
}
void RenderManager::SetUpGamePipeline(){


}

void RenderManager::Update(){
    SceneCamera::Get().Update();
    const auto& shader = SharedResources::Get().GetShaderByName("grid");
   
}

void RenderManager::Render(){
    auto& scene_manager = SceneManager::Get();
    for (auto& scene : scene_manager.GetScenes()){
        editor_pipeline->Render(SceneCamera::Get(), *scene);
        
    }
}