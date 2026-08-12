#pragma once
#include <cstdint>
#include <string>
#include <vector>

// CPU-side image decode, split out from Texture2D so it can run off the main
// thread. The same shape FontAtlasBaker already established: a free function
// taking a path, returning a self-contained value type, touching nothing else.
//
// Before this existed, Texture2D::Awake did stbi_load and glTexImage2D in one
// function and freed the pixels immediately, so there was no seam to thread at
// and no buffer to hand across one. (It also meant Sprite::RebuildOpaqueOutline
// and Texture2D::RebuildNormalMap had to re-read the same PNG off disk, each
// with a comment saying so.)
struct DecodedImage {
    bool ok = false;
    std::string error;
    int width = 0, height = 0, channels = 0;
    std::vector<unsigned char> pixels;   // tightly packed, `channels` bytes/texel
};

namespace ImageDecoder {

// Pure CPU work -- no GL, safe to call without a current context, safe to call
// from several threads at once. Never throws; failure comes back as ok == false
// with `error` set.
//
// Thread safety hinges on one detail: this uses
// stbi_set_flip_vertically_on_load_thread, not the plain setter. The plain one
// writes a process-global that concurrent decodes would race, silently flipping
// each other's images. STBI_THREAD_LOCAL is active in this build (C++20 path,
// and STBI_NO_THREAD_LOCALS is not defined anywhere), so the _thread variant is
// genuinely per-thread.
DecodedImage Decode(const std::string& path, bool flipVertically = true);

} // namespace ImageDecoder
