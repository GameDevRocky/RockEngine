#pragma once
#include <string>
#include <vector>
#include <limits>

namespace Properties {

    enum class Tags {
        NONE,
        COLOR,
        SLIDER,
        ANGLE,
        FILEPATH,
        VECTOR2,
        VECTOR3,
        FLOAT,
        INT,
        TOGGLE,
        MULTILINE,
        READONLY
    };

    struct PropDesc {
        Tags tag = Tags::NONE;
        float min = -std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::max();
        float step = 0.1f;
        std::string suffix = "";
        std::string description = "";

        PropDesc& Range(float lo, float hi) { min = lo; max = hi; return *this; }
        PropDesc& Suffix(const std::string& s) { suffix = s; return *this; }
        PropDesc& Desc(const std::string& d)   { description = d; return *this; }
        PropDesc& Tag(Tags t)                  { tag = t; return *this; }
    };
}