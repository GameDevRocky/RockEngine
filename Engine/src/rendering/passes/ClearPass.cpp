#include "engine/rendering/passes/ClearPass.hpp"




void ClearPass::Execute(RenderCamera* camera, Scene* scene){
    glad_glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}