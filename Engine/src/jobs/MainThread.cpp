#include "engine/jobs/MainThread.hpp"

#include <atomic>
#include <thread>

namespace {

std::thread::id g_mainThread;
std::atomic<bool> g_stamped{false};

}

namespace MainThread {

void Stamp() {
    g_mainThread = std::this_thread::get_id();
    g_stamped.store(true, std::memory_order_release);
}

bool IsCurrent() {
    if (!g_stamped.load(std::memory_order_acquire)) return true;   // not yet armed
    return std::this_thread::get_id() == g_mainThread;
}

const char* Name() {
    return IsCurrent() ? "main" : "worker";
}

} // namespace MainThread
