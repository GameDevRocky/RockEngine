#pragma once
#include <memory>

class SceneEditTool;

std::unique_ptr<SceneEditTool> CreateGizmoEditTool();
std::unique_ptr<SceneEditTool> CreateTilemapPaintTool();
