#include "engine/rendering/passes/ClearPass.hpp"

void ClearPass::Execute(RenderCamera* camera, Scene* scene){
    if (!camera) return;

    switch (camera->GetClearFlags())
    {
        case RenderCamera::ClearFlags::SolidColor:
        {
            const glm::vec4& c = camera->GetClearColor();
            glad_glClearColor(c.r, c.g, c.b, c.a);
            glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            break;
        }
        case RenderCamera::ClearFlags::DepthOnly:
            glad_glClear(GL_DEPTH_BUFFER_BIT);
            break;
        case RenderCamera::ClearFlags::Nothing:
            break;
    }
}