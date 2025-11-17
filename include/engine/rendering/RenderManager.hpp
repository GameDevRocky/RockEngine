#pragma once
#include <glad/glad.h>
#include "engine/core/System.hpp"
#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include "engine/rendering/cameras/GameCamera.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"

class RenderManager : public System{

public:

static RenderManager& Get() {
    static RenderManager instance;
    return instance;
}

void Init();

void SetUpEditorPipeline();
void SetUpGamePipeline();
void Update() override;
void Render();

RenderPipeline* editor_pipeline;
RenderPipeline* game_pipeline;
private:
RenderManager() = default;

};