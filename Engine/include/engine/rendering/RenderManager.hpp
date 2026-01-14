#pragma once
#include <glad/glad.h>
#include "engine/core/System.hpp"
#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include "engine/rendering/cameras/GameCamera.hpp"
#include "engine/rendering/cameras/SceneCamera.hpp"

class RenderManager{

public:

RenderManager() = default;

void Init();
void Update();

void SetUpEditorPipeline();
void SetUpGamePipeline();
void Render();

RenderPipeline* editor_pipeline;
RenderPipeline* game_pipeline;
private:

};