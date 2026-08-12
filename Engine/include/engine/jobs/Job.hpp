#pragma once
#include <cstdint>
#include <functional>
#include <string>

// Long-operation plumbing: the value types a caller fills in to describe one
// unit of work. The scheduler that runs them is JobSystem.
//
// The whole design exists because of one fact about this engine: it is
// single-threaded by construction and hostile to background mutation.
// Observable::Notify has no locks and runs Qt widget code inline; Registry has
// no locks and stamps caches with a memory_order_relaxed generation counter
// whose header says "everything runs on the GUI thread"; and
// EngineUtils::GenerateUUID shares a static std::mt19937 that EVERY
// Serializable constructor calls. So `new GameObject` on a worker thread is
// already a data race.
//
// Rather than retrofit locks onto all of that -- which would signal that
// touching engine state from a worker is fine, and invite exactly the code this
// forbids -- a job is split in two halves with a hard rule about each.

using JobId = std::uint64_t;   // 0 == invalid / not a real job

enum class JobState {
    Queued,          // submitted; the worker has not picked it up yet
    RunningWorker,   // the pure-CPU half is on the worker thread
    RunningMain,     // the main-thread step is being pumped
    Succeeded,
    Failed,
    // Deliberately no Cancelled. Cancelling a half-populated Registry or a
    // half-copied Container means writing a rollback path for the most delicate
    // code in the engine, and rollback bugs are worse than waiting 800ms.
};

struct JobRecord;   // JobSystem's internal bookkeeping; opaque here.

// The one progress channel, handed to both halves so the UI reads a single
// field regardless of which half is currently running. Writes take the record's
// mutex. Nothing here ever touches Observable -- see JobSystem's header for why
// progress is a pull rather than a notify.
class JobProgressSink {
public:
    // fraction in [0,1]; pass a negative value for "indeterminate" (a busy
    // sweep rather than a filled bar).
    void Report(float fraction, std::string label);

    // Abandon the job. A worker should return immediately afterwards. A failed
    // worker half SKIPS the main step entirely and goes straight to the
    // completion callback -- half a result must never be applied to the graph.
    void Fail(std::string error);

    bool Failed() const;

private:
    friend class JobSystem;
    explicit JobProgressSink(JobRecord* rec) : m_rec(rec) {}
    JobRecord* m_rec;
};

// ─────────────────────────────────────────────────────────────────────────────
// The pure-CPU half. Runs on the worker thread.
//
// FORBIDDEN inside a WorkerFn, without exception:
//   - Observable::Notify / Subscribe   (no locks; runs Qt handlers inline)
//   - Console::Alert / Warn / Comment  (unguarded map, notifies into Qt)
//   - Registry::*, Engine::Get(), AssetManager::Get()
//   - EngineUtils::GenerateUUID(), and therefore constructing ANYTHING deriving
//     from Serializable -- the shared mt19937 is not thread-safe
//   - any gl* call, any pybind11 call, any Qt call
//
// A WorkerFn may touch only what it captured by value and its own result
// buffer. It need not be noexcept -- JobSystem catches and converts to Fail --
// but returning cleanly after sink.Fail() is the intended shape.
using JobWorkerFn = std::function<void(JobProgressSink&)>;

// The main-thread half. Return true if there is more work to do (it will be
// called again next slice), false when finished. Runs only after the worker
// half succeeded.
//
// NOT a place for GL. The pump runs from Engine::Update(), which Editor calls
// outside paintGL -- and which the stall watchdog also calls when no viewport is
// presenting at all, where no context is current by definition. A job installs a
// decoded *result*; the existing draw-path hooks (Font::EnsureUploaded, polled
// from ScenePass) do the upload where a context is guaranteed.
using JobMainStepFn = std::function<bool(JobProgressSink&)>;

struct JobDesc {
    std::string title;          // "Loading scene: level_01" -- shown to the user
    JobWorkerFn worker;         // optional: omit for main-thread-only work
    JobMainStepFn mainStep;     // optional: runs after the worker half succeeds

    // Does this job raise the modal loading overlay? False for background work
    // the user should never be blocked by -- a font atlas bake finishes on its
    // own and the text simply appears.
    bool modal = true;

    // How long the job must be alive before the overlay appears. Stops fast work
    // from flashing a card. Set to 0 for an operation that is KNOWN to block --
    // there the card is the whole point and a delay just hides it.
    int graceMs = 150;

    // Pumps to skip before the main step is allowed to run.
    //
    // This exists for un-threadable work. A job with no worker half would
    // otherwise be drained and executed inside the same Pump() call, so the
    // frame loop never returns to Qt and the overlay never gets painted before
    // the stall it is supposed to explain. Two pumps is enough for one
    // show() and one paint. Costs ~2 frames (~30ms at 60Hz) -- worth it to turn
    // a frozen window into a labelled one.
    int warmupPumps = 0;

    // Main thread, once, after the job reaches a terminal state. This is where
    // Console::Alert and Notify belong.
    std::function<void(bool ok, const std::string& error)> onFinished;
};

// A copied-out snapshot. The UI never holds a pointer into JobSystem.
struct JobStatus {
    JobId       id = 0;
    std::string title;
    std::string label;
    float       fraction = -1.0f;   // negative == indeterminate
    JobState    state = JobState::Queued;
    bool        modal = true;
    int         graceMs = 150;
};
