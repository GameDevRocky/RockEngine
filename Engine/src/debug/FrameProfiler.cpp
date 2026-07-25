#include "engine/debug/FrameProfiler.hpp"
#include <cstdio>
#include <cstring>

FrameProfiler& FrameProfiler::Get() {
    static FrameProfiler instance;
    return instance;
}

FrameProfiler::Entry* FrameProfiler::FindOrAdd(const char* name) {
    for (int i = 0; i < entryCount; ++i) {
        // Pointer compare first: section names are string literals, usually merged.
        if (entries[i].name == name || std::strcmp(entries[i].name, name) == 0)
            return &entries[i];
    }
    if (entryCount >= kMaxSections) return nullptr;
    entries[entryCount].name = name;
    return &entries[entryCount++];
}

void FrameProfiler::Add(const char* section, std::uint64_t micros) {
    if (Entry* e = FindOrAdd(section)) {
        e->micros += micros;
        e->count += 1;
    }
}

void FrameProfiler::Count(const char* section, std::uint64_t n) {
    if (Entry* e = FindOrAdd(section)) e->count += n;
}

void FrameProfiler::FrameBoundary() {
    const auto now = std::chrono::steady_clock::now();
    if (!windowStarted) {
        windowStarted = true;
        windowStart = now;
    }
    ++frames;
    if (frames < kReportFrames) return;

    const double windowMs = std::chrono::duration<double, std::milli>(now - windowStart).count();
    const double frameMs = windowMs / static_cast<double>(frames);
    const double fps = frameMs > 0.0 ? 1000.0 / frameMs : 0.0;

    std::printf("[FrameProfiler] %llu frames | avg %.2f ms/frame (%.1f fps)\n",
                static_cast<unsigned long long>(frames), frameMs, fps);
    for (int i = 0; i < entryCount; ++i) {
        const Entry& e = entries[i];
        std::printf("  %-28s avg %8.1f us/frame  (%.1f calls/frame)\n",
                    e.name,
                    static_cast<double>(e.micros) / static_cast<double>(frames),
                    static_cast<double>(e.count) / static_cast<double>(frames));
        }
    std::fflush(stdout);

    for (int i = 0; i < entryCount; ++i) {
        entries[i].micros = 0;
        entries[i].count = 0;
    }
    frames = 0;
    windowStart = now;
}
