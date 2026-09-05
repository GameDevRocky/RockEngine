// Entry point for the headless test binary. doctest generates the runner from
// DOCTEST_CONFIG_IMPLEMENT; every other file in Tests/ includes doctest.h WITHOUT it.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include "engine/jobs/MainThread.hpp"

int main(int argc, char** argv) {
    // Observable::Notify (and Registry, Console, GenerateUUID) assert they are on the
    // thread that owns the engine -- ROCK_ASSERT_MAIN_THREAD, live in debug builds, which
    // is exactly what CI builds. Engine::Init normally does this stamping; these tests
    // deliberately never call Engine::Init, so the test runner claims the thread instead.
    // Without this every Notify() in the suite trips the assert.
    MainThread::Stamp();

    doctest::Context context(argc, argv);
    return context.run();
}
