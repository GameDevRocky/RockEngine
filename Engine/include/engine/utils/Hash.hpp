#pragma once

#include <cstddef>
#include <functional>
#include <glm/vec2.hpp>

namespace EngineUtils {

// Hash integer 2D coordinates for unordered containers.
struct IVec2Hash {
    std::size_t operator()(const glm::ivec2& value) const noexcept {
        std::size_t seed = std::hash<int>{}(value.x);
        seed ^= std::hash<int>{}(value.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

} // namespace EngineUtils
