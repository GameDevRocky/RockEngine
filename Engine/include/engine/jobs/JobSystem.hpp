#pragma once
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "engine/jobs/Job.hpp"

// Runs long operations without freezing the editor.
//
// ── Where it lives ──────────────────────────────────────────────────────────
// A process-global singleton OUTSIDE any Container, alongside Renderer and
// AssetManager. That is structural, not stylistic:
//   - Container::Copy() copies every System, so a JobSystem-as-System would be
//     duplicated on EnterPlayMode -- two queues, two worker threads.
//   - Container::Shutdown() deletes every System, so the job that is ENTERING
//     play mode would be executing out of an object ExitPlayMode later frees.
//   - Registry::Copy deep-copies every RuntimeObject, and "copy the thread
//     pool" is nonsense.
// Not being a System also means Container::Update does not tick it, which is
// what we want: Pump() must run even at the instant activeContainer is swapped,
// so it is called explicitly from Engine::Update where that ordering is visible.
//
// ── Deliberately NOT an Observable ──────────────────────────────────────────
// Progress is a PULL: the editor calls SnapshotActive() from its own per-frame
// update and gets copied-out values. Pushing progress through Observable would
// put a notify on the same path as worker completion, and Observable::Notify is
// unlocked and runs Qt handlers inline. Keeping it out of the design means the
// "a worker must never Notify" rule holds by construction rather than by
// discipline. This is the same pull-don't-push argument the renderer already
// makes for resolving cameras at render time.
class JobSystem {
public:
    static JobSystem& Get();

    // Main thread only. Returns immediately.
    JobId Submit(JobDesc desc);

    // Main thread only, once per frame from Engine::Update(), BEFORE the active
    // container ticks -- so a container-swapping job completes before anything
    // reads the container that frame. Non-reentrant.
    void Pump();

    // Is anything queued, running, or awaiting its main step?
    bool Busy() const;
    // ...and specifically anything that should raise the modal overlay.
    bool HasActiveModalJob() const;

    // Copied-out state for the UI, newest last.
    std::vector<JobStatus> SnapshotActive() const;

    // Stop accepting work, join the worker, discard un-installed results.
    // Idempotent. Must run before Qt is torn down, so the editor calls it first
    // thing in its own Shutdown; Engine::Shutdown calls it again as a net for a
    // headless embedding.
    void Shutdown();

private:
    JobSystem();
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void WorkerLoop();
    void Finish(const std::shared_ptr<JobRecord>& rec, bool ok);

    // One worker in v1. Every worker half identified so far is serial, and a
    // single thread makes every race a two-party race with deterministic FIFO
    // order -- which matters a great deal when introducing the first concurrency
    // into a codebase whose every invariant assumes there is none.
    static constexpr int kWorkerThreads = 1;

    // Main-thread slice budget: ~6ms of a 16ms frame, leaving room for the rest
    // of Engine::Update, paintGL, and Qt. Tune this in Release -- MSVC Debug is
    // several times slower, so a budget tuned there buys nothing in Release.
    static constexpr int kMainSliceBudgetMs = 6;

    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;

    std::deque<std::shared_ptr<JobRecord>>  m_queued;      // main -> worker
    std::vector<std::shared_ptr<JobRecord>> m_workerDone;  // worker -> main
    std::vector<std::shared_ptr<JobRecord>> m_active;      // everything live

    // Main-thread only; never touched by the worker, so it needs no lock.
    std::vector<std::shared_ptr<JobRecord>> m_mainQueue;

    std::vector<std::thread> m_workers;
    bool  m_stopping = false;
    bool  m_pumping  = false;   // reentrancy guard -- see Pump()
    JobId m_nextId   = 1;
};
