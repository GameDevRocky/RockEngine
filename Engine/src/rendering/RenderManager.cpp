
#include "engine/rendering/RenderManager.hpp"
#include "engine/rendering/passes/GridPass.hpp"
#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/passes/ClearPass.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"
#include <glad/glad.h>

void RenderManager::Init(){
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }
    glEnable(GL_DEPTH_TEST);
    
    editor_pipeline = new RenderPipeline();
    game_pipeline = new RenderPipeline();
    
    SetUpEditorPipeline();
    SetUpGamePipeline();
}


void RenderManager::SetUpEditorPipeline(){
    auto clear_pass = new ClearPass();
    auto grid_pass = new GridPass();
    auto scene_pass = new ScenePass();

    editor_pipeline->AddPass(clear_pass);
    editor_pipeline->AddPass(grid_pass);
    editor_pipeline->AddPass(scene_pass);

    editor_pipeline->Init();
}
void RenderManager::SetUpGamePipeline(){


}

void RenderManager::Update(){
}

void RenderManager::Render(){
    
}