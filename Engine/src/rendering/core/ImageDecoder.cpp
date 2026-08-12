#include "engine/rendering/core/ImageDecoder.hpp"

// Header only -- STB_IMAGE_IMPLEMENTATION lives in Texture2D.cpp.
#include <stb_image.h>

namespace ImageDecoder {

DecodedImage Decode(const std::string& path, bool flipVertically) {
    DecodedImage out;

    // The _thread variant, NOT stbi_set_flip_vertically_on_load. The plain
    // setter writes a process-global that two concurrent decodes would race,
    // and the symptom would be images silently flipping each other -- a bug that
    // only shows up under load and looks like a content problem, not a threading
    // one. See the header for why the thread-local path is available here.
    stbi_set_flip_vertically_on_load_thread(flipVertically ? 1 : 0);

    // 0 desired channels: keep the file's native channel count, which is what
    // the GL format is derived from at upload time.
    unsigned char* data = stbi_load(path.c_str(), &out.width, &out.height, &out.channels, 0);
    if (!data) {
        const char* why = stbi_failure_reason();   // already thread-local in stb
        out.error = "failed to decode '" + path + "': " + (why ? why : "unknown");
        return out;
    }

    const std::size_t bytes =
        static_cast<std::size_t>(out.width) *
        static_cast<std::size_t>(out.height) *
        static_cast<std::size_t>(out.channels);

    // Copied into a vector rather than handing back stb's raw pointer: the
    // buffer has to outlive this call and cross a thread boundary, and an owning
    // value type is the only version of that which cannot leak or dangle.
    out.pixels.assign(data, data + bytes);
    stbi_image_free(data);

    out.ok = true;
    return out;
}

} // namespace ImageDecoder
