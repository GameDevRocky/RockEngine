#include "engine/rendering/passes/ClearPass.hpp"




void ClearPass::Execute(RenderCamera* camera, Scene* scene){
    glad_glClearColor(68.0f/255.0f, 89.0f/255.0f, 74.0f/255.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}