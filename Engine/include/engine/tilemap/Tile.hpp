#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

using TileId = std::uint16_t;

inline constexpr TileId EMPTY_TILE_ID = 0;
enum class TileCollisionType : std::uint8_t {
    None,
    Sprite,
    Grid,
    Custom
};

enum class TileOutputType : std::uint8_t {
    Fixed,
    Random,
    Animation
};

using NeighborMask = std::array<TileId, 8>;
using NeighborPattern = std::array<std::optional<TileId>, 8>;
using TileCollisionPolygon = std::vector<glm::vec2>;

inline TileCollisionPolygon DefaultTileCollisionPolygon() {
    return {{-0.5f, -0.5f}, {0.5f, -0.5f},
            {0.5f, 0.5f}, {-0.5f, 0.5f}};
}

inline bool IsValidTileCollisionPolygon(const TileCollisionPolygon& points) {
    if (points.size() < 3 || points.size() > 8) return false;
    for (glm::vec2 point : points)
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < -0.5f || point.x > 0.5f ||
            point.y < -0.5f || point.y > 0.5f) return false;
    constexpr float epsilon = 1.0e-6f;
    float signedArea = 0.0f;
    int winding = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const glm::vec2 a = points[i];
        const glm::vec2 b = points[(i + 1) % points.size()];
        const glm::vec2 c = points[(i + 2) % points.size()];
        if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
            glm::dot(b - a, b - a) <= epsilon) return false;
        signedArea += a.x * b.y - b.x * a.y;
        const float cross = (b.x - a.x) * (c.y - b.y) -
                            (b.y - a.y) * (c.x - b.x);
        if (std::abs(cross) <= epsilon) continue;
        const int turn = cross > 0.0f ? 1 : -1;
        if (winding && winding != turn) return false;
        winding = turn;
    }
    if (winding == 0 || std::abs(signedArea) <= epsilon) return false;
    // Checking only adjacent turns also accepts self-crossing star polygons.
    for (std::size_t i = 0; i < points.size(); ++i) {
        const glm::vec2 a = points[i];
        const glm::vec2 edge = points[(i + 1) % points.size()] - a;
        for (const glm::vec2 point : points) {
            const glm::vec2 delta = point - a;
            const float cross = edge.x * delta.y - edge.y * delta.x;
            if (cross * winding < -epsilon) return false;
        }
    }
    return true;
}

struct AutoTileRule {
    NeighborPattern neighbors{};
    std::string spriteId;
    TileCollisionType collisionType = TileCollisionType::None;
    TileCollisionPolygon customPolygon = DefaultTileCollisionPolygon();
    TileOutputType outputType = TileOutputType::Fixed;
    std::vector<std::string> spriteIds;
    float minAnimationSpeed = 1.0f;
    float maxAnimationSpeed = 1.0f;
    bool operator==(const AutoTileRule&) const = default;
};

struct ResolvedTile {
    std::string spriteId;
    TileCollisionType collisionType = TileCollisionType::None;
    TileOutputType outputType = TileOutputType::Fixed;
    // Non-owning; valid while the TileSet is unchanged.
    const TileCollisionPolygon* customPolygon = nullptr;
};

struct Tile {
    std::string name;
    std::string defaultSpriteId;
    TileCollisionType collisionType = TileCollisionType::None;
    TileCollisionPolygon customPolygon = DefaultTileCollisionPolygon();
    std::vector<AutoTileRule> autoTileRules;
};
