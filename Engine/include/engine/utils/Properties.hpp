#pragma once
#include <string>
#include <vector>
#include <limits>
#include <any>

namespace Properties {

    enum class Tags {
        NONE,
        COLOR,
        SLIDER,
        ANGLE,
        FILEPATH,
        VECTOR2,
        VECTOR3,
        VECTOR4,
        FLOAT,
        INT,
        TOGGLE,
        MULTILINE,
        READONLY,
        MATERIAL,
        SPRITE,
        OBJECT_REF,
        DROPDOWN
    };

    struct PropDesc {
        Tags tag = Tags::NONE;
        Tags refType = Tags::NONE;
        float min = -std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::max();
        float step = 0.1f;
        std::string description = "";
        std::string refClassFilter = "";  // For OBJECT_REF: filters GameObjects to those with this script class

        std::vector<std::pair<std::string, std::any>> dropdownOptions;

        PropDesc& Range(float lo, float hi) { min = lo; max = hi; return *this; }
        PropDesc& Desc(const std::string& d)   { description = d; return *this; }
        PropDesc& Tag(Tags t)                  { tag = t; return *this; }
        PropDesc& RefType(Tags t)              { refType = t; return *this; }
        PropDesc& Step(float s)                  { step = s; return *this; }
        PropDesc& RefClass(const std::string& cls) { refClassFilter = cls; return *this; }
        PropDesc& DropVals(std::vector<std::pair<std::string, std::any>> vals) {
            dropdownOptions = std::move(vals);
            return *this;
        }
    };
}