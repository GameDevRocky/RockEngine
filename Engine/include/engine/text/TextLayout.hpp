#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Font;

// Where the text block sits relative to the GameObject's origin.
enum class TextHAlign { Left, Center, Right };
enum class TextVAlign { Top, Middle, Baseline, Bottom };

// Everything about a block of text that affects where its glyphs land.
// Deliberately separate from TextRenderer: this struct is the whole input to
// layout, which makes the layout code testable on its own and gives the mesh
// cache an obvious thing to hash.
struct TextLayoutSpec {
    float fontSize      = 32.0f;   // world units per em
    float lineSpacing   = 1.0f;    // multiplier on the face's own line height
    float letterSpacing = 0.0f;    // extra tracking, em
    float maxWidth      = 0.0f;    // world units; 0 disables wrapping
    TextHAlign hAlign   = TextHAlign::Center;
    TextVAlign vAlign   = TextVAlign::Middle;
};

// One positioned glyph, in text-local world units (the object's own space,
// before its Transform). Emitted ready to become two triangles.
struct TextQuad {
    glm::vec2 min{ 0.0f }, max{ 0.0f };
    glm::vec2 uvMin{ 0.0f }, uvMax{ 0.0f };
};

struct TextMesh {
    std::vector<TextQuad> quads;

    // Block bounds in the same local units. Derived from the face's ascender and
    // descender and the laid-out line widths -- NOT from the ink of the glyphs
    // actually present. That is what stops the box (and anything anchored to it)
    // from jumping around as the user types, or as a line happens to contain a
    // descender.
    glm::vec2 boundsMin{ 0.0f }, boundsMax{ 0.0f };
    int lineCount = 0;
};

// Pure text shaping: no GL, no msdfgen, no engine singletons. The editor calls
// this for gizmo and pick bounds, the mesh cache calls it to build vertices, and
// a future screen-space canvas would call the very same function -- it is
// camera-agnostic and knows nothing about world transforms.
namespace TextLayout {

    // Lays the string out. Returns an empty mesh (all zero bounds) for a null or
    // unbaked font, or an empty string.
    TextMesh Build(const Font* font, const std::string& utf8, const TextLayoutSpec& spec);

    // Bounds without building quads, for picking and gizmos.
    void Measure(const Font* font, const std::string& utf8, const TextLayoutSpec& spec,
                 glm::vec2& outMin, glm::vec2& outMax);
}
