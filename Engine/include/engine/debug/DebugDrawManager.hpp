#pragma once
#include "engine/core/System.hpp"
#include <glm/glm.hpp>
#include <vector>

struct DebugDrawCommand
{
    enum class Type { Point, Line, Box, Circle };

    Type      type;
    glm::vec2 a;        
    glm::vec2 b; 
    float     rotation; // box rotation in degrees (unused for others)
    float     radius;   // circle radius / point radius
    glm::vec4 color;
};

class DebugDrawManager : public System
{
public:
    DebugDrawManager(const DebugDrawManager&)            = delete;
    DebugDrawManager& operator=(const DebugDrawManager&) = delete;

    static DebugDrawManager* Get()
    {
        static DebugDrawManager instance;
        return &instance;
    }

    void Init()     override;
    void Update()   override;
    void Shutdown() override;

    DebugDrawManager* Copy() override;
    DebugDrawManager* Copy(Container* container) override;

    // Called by DebugPass after consuming commands each frame
    void Clear();

    // Pixel-space API (matching the rest of the engine's coordinate convention)
    void DrawPoint(const glm::vec2& pos,     const glm::vec4& color = glm::vec4(1, 1, 0, 1));
    void DrawLine(const glm::vec2& start,    const glm::vec2& end,
                  const glm::vec4& color = glm::vec4(1, 1, 0, 1));
    void DrawBox(const glm::vec2& center,    const glm::vec2& size, float rotation = 0.0f,
                 const glm::vec4& color = glm::vec4(1, 1, 0, 1));
    void DrawCircle(const glm::vec2& center, float radius,
                    const glm::vec4& color = glm::vec4(1, 1, 0, 1));

    const std::vector<DebugDrawCommand>& GetCommands() const { return commands; }

private:
    DebugDrawManager() = default;

    std::vector<DebugDrawCommand> commands;
};
