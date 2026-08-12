#include "engine/jobs/BootProgress.hpp"

namespace {
BootProgress::Fn g_sink;
}

namespace BootProgress {

void SetSink(Fn fn) { g_sink = std::move(fn); }

void Report(float fraction, const std::string& label) {
    if (g_sink) g_sink(fraction, label);
}

} // namespace BootProgress
