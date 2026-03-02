#include "engine/rendering/passes/ClearPass.hpp"




void ClearPass::Execute(RenderCamera* camera, Scene* scene){
    glad_glClearColor(0.2f, 0.25f, 0.3f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}