#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

// A blocking parallel loop. Deliberately NOT part of JobSystem.
//
// JobSystem is asynchronous and its completion pump rides the frame loop, which
// makes it useless for the one place a batch fan-out is most valuable: the
// startup asset load runs inside the first viewport's initializeGL, before
// app->exec(), so there is no frame loop to pump. This is the primitive for
// that case -- spawn, run, join, return, all inside one call.
//
// Same rule as a job's worker half: `body` must be pure CPU. No Observable, no
// Console, no Registry, no AssetManager, no GenerateUUID, no GL, no Python. The
// caller is responsible for making sure each index touches only its own slot of
// whatever it writes into -- there is no synchronization here beyond the join.
namespace ParallelFor {

// Runs body(i) for i in [0, count), across hardware_concurrency threads.
// Runs inline on the calling thread when count is small or only one core is
// available, so the cost is never worse than the serial loop.
inline void Run(std::size_t count, const std::function<void(std::size_t)>& body) {
    if (count == 0) return;

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workers = std::min<std::size_t>(hw, count);

    if (workers <= 1 || count < 4) {
        for (std::size_t i = 0; i < count; ++i) body(i);
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(workers - 1);

    // Strided rather than blocked: image decode cost varies wildly with
    // dimensions, and a blocked split would leave one thread holding all the
    // large files while the others idle.
    auto run = [&body, count, workers](std::size_t start) {
        for (std::size_t i = start; i < count; i += workers) body(i);
    };

    for (std::size_t t = 1; t < workers; ++t)
        threads.emplace_back([run, t]() { run(t); });

    run(0);   // the calling thread takes a share too

    for (std::thread& th : threads) th.join();
}

} // namespace ParallelFor
