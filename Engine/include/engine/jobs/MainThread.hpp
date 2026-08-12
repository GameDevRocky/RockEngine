#pragma once

// A debug-only tripwire for the one rule the job system depends on: a worker
// thread must never touch engine state.
//
// The structures a background thread would corrupt -- Observable's subscriber
// map, Registry's object map and its relaxed generation counter, Console's
// message map, GenerateUUID's shared mt19937 -- have no locks and never will;
// the whole design is that workers stay away from them rather than that they
// become thread-safe. That rule is only as good as its enforcement, and a
// violation otherwise shows up as a rare corrupted map on someone else's
// machine. Stamping the main thread once and asserting at the handful of
// choke points turns it into an immediate, local crash.
//
// Compiled out entirely in release: ROCK_ASSERT_MAIN_THREAD costs one
// thread-id comparison in debug and literally nothing otherwise.
namespace MainThread {

// Called once from Engine::Init, on the thread that owns the engine.
void Stamp();

// False before Stamp() (so early static init never trips the assert).
bool IsCurrent();

// Human-readable, for the assert message.
const char* Name();

} // namespace MainThread

#ifndef NDEBUG
    #include <cassert>
    #define ROCK_ASSERT_MAIN_THREAD() \
        assert(MainThread::IsCurrent() && "engine state touched from a worker thread")
#else
    #define ROCK_ASSERT_MAIN_THREAD() ((void)0)
#endif
