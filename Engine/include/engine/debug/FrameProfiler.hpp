#pragma once
#include <chrono>
#include <cstdint>

// Lightweight per-frame section profiler. STL-only, single-threaded (everything
// runs on the GUI thread: Engine::Update and Qt paintGL alike). Sections are
// accumulated per frame and averages are printed to stdout every report window.
// Define ROCKENGINE_PROFILING=0 to compile all instrumentation out.
#ifndef ROCKENGINE_PROFILING
#define ROCKENGINE_PROFILING 0
#endif

class FrameProfiler {
public:
    static FrameProfiler& Get();

    // Call once per engine frame; prints averages and resets every window.
    void FrameBoundary();
    void Add(const char* section, std::uint64_t micros);
    void Count(const char* section, std::uint64_t n = 1);

    class Scope {
    public:
        explicit Scope(const char* name)
            : name(name), start(std::chrono::steady_clock::now()) {}
        ~Scope() {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
            FrameProfiler::Get().Add(name, static_cast<std::uint64_t>(us));
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        const char* name;
        std::chrono::steady_clock::time_point start;
    };

private:
    static constexpr int kMaxSections = 32;
    static constexpr std::uint64_t kReportFrames = 120;

    struct Entry {
        const char* name = nullptr;
        std::uint64_t micros = 0;
        std::uint64_t count = 0;
    };

    Entry* FindOrAdd(const char* name);

    Entry entries[kMaxSections];
    int entryCount = 0;
    std::uint64_t frames = 0;
    std::chrono::steady_clock::time_point windowStart{};
    bool windowStarted = false;
};

#if ROCKENGINE_PROFILING
#define ROCK_PROFILE_CONCAT2(a, b) a##b
#define ROCK_PROFILE_CONCAT(a, b) ROCK_PROFILE_CONCAT2(a, b)
#define ROCK_PROFILE_SCOPE(name) \
    FrameProfiler::Scope ROCK_PROFILE_CONCAT(rockProfScope_, __LINE__){name}
#define ROCK_PROFILE_COUNT(name, n) FrameProfiler::Get().Count(name, n)
#define ROCK_PROFILE_FRAME() FrameProfiler::Get().FrameBoundary()
#else
#define ROCK_PROFILE_SCOPE(name) ((void)0)
#define ROCK_PROFILE_COUNT(name, n) ((void)0)
#define ROCK_PROFILE_FRAME() ((void)0)
#endif
