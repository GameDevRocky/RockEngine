#include "engine/debug/DebugDrawManager.hpp"

void DebugDrawManager::Init()
{
    commands.clear();
}

void DebugDrawManager::Update()
{
}

void DebugDrawManager::Shutdown()
{
    commands.clear();
}

DebugDrawManager* DebugDrawManager::Copy()
{
    return Get();
}

DebugDrawManager* DebugDrawManager::Copy(Container* container)
{
    auto* i = Copy();
    i->Attach(container);
    return i;
}

void DebugDrawManager::Clear()
{
    commands.clear();
}

void DebugDrawManager::DrawPoint(const glm::vec2& pos, const glm::vec4& color)
{
    DebugDrawCommand cmd{};
    cmd.type   = DebugDrawCommand::Type::Point;
    cmd.a      = pos;
    cmd.radius = 1.0f;
    cmd.color  = color;
    commands.push_back(cmd);
}

void DebugDrawManager::DrawLine(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color)
{
    DebugDrawCommand cmd{};
    cmd.type  = DebugDrawCommand::Type::Line;
    cmd.a     = start;
    cmd.b     = end;
    cmd.color = color;
    commands.push_back(cmd);
}

void DebugDrawManager::DrawBox(const glm::vec2& center, const glm::vec2& size, float rotation, const glm::vec4& color)
{
    DebugDrawCommand cmd{};
    cmd.type     = DebugDrawCommand::Type::Box;
    cmd.a        = center;
    cmd.b        = size;
    cmd.rotation = rotation;
    cmd.color    = color;
    commands.push_back(cmd);
}

void DebugDrawManager::DrawCircle(const glm::vec2& center, float radius, const glm::vec4& color)
{
    DebugDrawCommand cmd{};
    cmd.type   = DebugDrawCommand::Type::Circle;
    cmd.a      = center;
    cmd.radius = radius;
    cmd.color  = color;
    commands.push_back(cmd);
}
