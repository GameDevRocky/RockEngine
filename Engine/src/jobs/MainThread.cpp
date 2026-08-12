#include "engine/jobs/MainThread.hpp"

#include <atomic>
#include <thread>

namespace {

std::thread::id g_mainThread;

// Separate from the id because a default-constructed thread::id is a valid
// "no thread" value that nothing compares equal to -- but relying on that would
// make IsCurrent() return false during static init, before Stamp() runs, and
// trip asserts in constructors that legitimately run on the main thread.
std::atomic<bool> g_stamped{false};

} // namespace

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
