#pragma once
#include <string>
#include <vector>
#include <limits>
#include <any>
#include <memory>

namespace Properties {

    enum class Tags {
        NONE,
        COLOR,
        SLIDER,        // float, dragged along a bounded Range
        RANGE_SLIDER,  // vec2 (x = low, y = high), two handles on one bounded Range
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
        TEXTURE,
        SHADER,
        FONT,
        AUDIO_CLIP,
        STRING,
        OBJECT_REF,
        SCRIPT,
        CALL_ENTRY,    // one wired call inside an Event field (target + method + arg)
        DROPDOWN,
        LIST
    };

    struct PropDesc {
        Tags tag = Tags::NONE;
        Tags refType = Tags::NONE;
        float min = -std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::max();
        float step = 0.1f;
        std::string description = "";
        std::string refClassFilter = "";  // For OBJECT_REF: filters GameObjects to those with this script class
        std::string componentTypeFilter = "";  // For OBJECT_REF: filters GameObjects that have this native component type (e.g. "Camera")
        bool readOnly = false;            // Display-only. Tags::LIST: no add/remove buttons. Scalars: non-editable input.
        // Span both inspector columns, with the label on its own line above,
        // instead of sitting in the narrow value column beside it. For editors
        // too wide to be usable in a third of a panel — an Event's call rows put
        // a target picker, a method dropdown and an argument on one line.
        bool fullRow = false;

        std::vector<std::pair<std::string, std::any>> dropdownOptions;

        // For Tags::LIST: describes how each element row is rendered. A list
        // PropertyWidget creates one child widget per element via this descriptor.
        std::shared_ptr<PropDesc> elementDesc;

        PropDesc& Range(float lo, float hi) { min = lo; max = hi; return *this; }
        PropDesc& Desc(const std::string& d)   { description = d; return *this; }
        PropDesc& Tag(Tags t)                  { tag = t; return *this; }
        PropDesc& RefType(Tags t)              { refType = t; return *this; }
        PropDesc& Step(float s)                  { step = s; return *this; }
        PropDesc& RefClass(const std::string& cls) { refClassFilter = cls; return *this; }
        PropDesc& ComponentType(const std::string& t) { componentTypeFilter = t; return *this; }
        PropDesc& DropVals(std::vector<std::pair<std::string, std::any>> vals) {
            dropdownOptions = std::move(vals);
            return *this;
        }
        PropDesc& Element(const PropDesc& d) {
            elementDesc = std::make_shared<PropDesc>(d);
            return *this;
        }
        PropDesc& ReadOnly(bool v = true) { readOnly = v; return *this; }
        PropDesc& FullRow(bool v = true)  { fullRow = v; return *this; }
    };
}