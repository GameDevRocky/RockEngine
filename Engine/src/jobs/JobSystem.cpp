#include "engine/jobs/JobSystem.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

// The bookkeeping behind one submitted JobDesc. Lives in the .cpp because
// nothing outside the scheduler has any business reaching into it -- callers see
// JobDesc going in and JobStatus coming out.
struct JobRecord {
    JobId    id = 0;
    JobDesc  desc;

    // Guards everything below. Held only for the duration of a field read or
    // write, never across a call into user code.
    mutable std::mutex mutex;
    JobState    state = JobState::Queued;
    float       fraction = -1.0f;
    std::string label;
    std::string error;

    // Main-thread only: pumps still to skip before the main step may run.
    int warmupLeft = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
void JobProgressSink::Report(float fraction, std::string label) {
    if (!m_rec) return;
    std::lock_guard<std::mutex> lk(m_rec->mutex);
    // A Fail already recorded is final: a worker that reports progress on its
    // way out of a failure must not paint over the error.
    if (m_rec->state == JobState::Failed) return;
    m_rec->fraction = fraction;
    m_rec->label    = std::move(label);
}

void JobProgressSink::Fail(std::string error) {
    if (!m_rec) return;
    std::lock_guard<std::mutex> lk(m_rec->mutex);
    m_rec->state = JobState::Failed;
    m_rec->error = std::move(error);
}

bool JobProgressSink::Failed() const {
    if (!m_rec) return false;
    std::lock_guard<std::mutex> lk(m_rec->mutex);
    return m_rec->state == JobState::Failed;
}

// ─────────────────────────────────────────────────────────────────────────────
JobSystem& JobSystem::Get() {
    static JobSystem instance;
    return instance;
}

JobSystem::JobSystem() {
    m_workers.reserve(kWorkerThreads);
    for (int i = 0; i < kWorkerThreads; ++i)
        m_workers.emplace_back([this]() { WorkerLoop(); });
}

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Shutdown() {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stopping) return;      // idempotent
        m_stopping = true;
        // Drop anything not yet started. A job still running finishes its
        // current call -- there is no cancellation, and killing a worker
        // mid-bake is worse than waiting for it.
        m_queued.clear();
    }
    m_cv.notify_all();

    for (std::thread& t : m_workers)
        if (t.joinable()) t.join();
    m_workers.clear();

    // Results that never got installed are dropped on purpose: the engine they
    // would be installed into is going away.
    std::lock_guard<std::mutex> lk(m_mutex);
    m_workerDone.clear();
    m_mainQueue.clear();
    m_active.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
JobId JobSystem::Submit(JobDesc desc) {
    auto rec = std::make_shared<JobRecord>();
    rec->desc = std::move(desc);

    const bool hasWorker = static_cast<bool>(rec->desc.worker);

    // No pure-CPU half: it goes straight to the main-step pass, which runs on
    // the NEXT Pump -- i.e. the next frame -- so the overlay gets a chance to
    // paint before the work begins. That is exactly what the un-threadable
    // operations (play mode) need, and it falls out of the scheduling rather
    // than being special-cased. Stamped before publication, so no lock needed.
    if (!hasWorker) rec->state = JobState::RunningWorker;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stopping) return 0;
        rec->id = m_nextId++;
        m_active.push_back(rec);
        if (hasWorker) m_queued.push_back(rec);
        else           m_workerDone.push_back(rec);
    }

    if (hasWorker) m_cv.notify_one();
    return rec->id;
}

void JobSystem::WorkerLoop() {
    for (;;) {
        std::shared_ptr<JobRecord> rec;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this]() { return m_stopping || !m_queued.empty(); });
            if (m_stopping) return;
            rec = m_queued.front();
            m_queued.pop_front();
        }
        // Under the RECORD's mutex, not m_mutex: state is also read by
        // SnapshotActive on the main thread, and one field guarded by two
        // different locks is not guarded at all.
        {
            std::lock_guard<std::mutex> lk(rec->mutex);
            rec->state = JobState::RunningWorker;
        }

        JobProgressSink sink(rec.get());
        try {
            rec->desc.worker(sink);
        } catch (const std::exception& e) {
            sink.Fail(e.what());
        } catch (...) {
            sink.Fail("unknown exception in job worker");
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_workerDone.push_back(rec);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void JobSystem::Pump() {
    // Not optional. A main step that indirectly reaches Engine::Update -- a
    // nested Qt event loop, a modal dialog, processEvents -- would otherwise
    // recurse into this function while it is halfway through its own queues.
    if (m_pumping) return;
    m_pumping = true;

    // Drain the worker's output into a local under the lock, then release it
    // before running any user code. Same shape FileWatcherSystem::Update
    // already uses to drain the Python watcher queue.
    std::vector<std::shared_ptr<JobRecord>> done;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        done.swap(m_workerDone);
    }

    for (auto& rec : done) {
        bool failed = false;
        {
            std::lock_guard<std::mutex> lk(rec->mutex);
            failed = (rec->state == JobState::Failed);
        }
        if (failed) { Finish(rec, false); continue; }

        if (!rec->desc.mainStep) { Finish(rec, true); continue; }

        {
            std::lock_guard<std::mutex> lk(rec->mutex);
            rec->state = JobState::RunningMain;
        }
        rec->warmupLeft = rec->desc.warmupPumps;
        m_mainQueue.push_back(rec);
    }

    // Run main-thread steps until the budget runs out. A step that returns true
    // stays at the front and resumes next frame, which is what lets a long graph
    // mutation be spread across frames instead of blocking one.
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(kMainSliceBudgetMs);
    while (!m_mainQueue.empty() && std::chrono::steady_clock::now() < deadline) {
        std::shared_ptr<JobRecord> rec = m_mainQueue.front();

        // Not yet: give the UI its frames first. Without this a workerless job
        // would be drained and executed in this same call, and the overlay
        // meant to explain the stall would never get painted before it.
        if (rec->warmupLeft > 0) {
            --rec->warmupLeft;
            break;   // and don't let a later job jump the queue either
        }

        JobProgressSink sink(rec.get());

        bool more = false;
        try {
            more = rec->desc.mainStep(sink);
        } catch (const std::exception& e) {
            sink.Fail(e.what());
        } catch (...) {
            sink.Fail("unknown exception in job main step");
        }

        if (sink.Failed()) {
            m_mainQueue.erase(m_mainQueue.begin());
            Finish(rec, false);
        } else if (!more) {
            m_mainQueue.erase(m_mainQueue.begin());
            Finish(rec, true);
        }
    }

    m_pumping = false;
}

void JobSystem::Finish(const std::shared_ptr<JobRecord>& rec, bool ok) {
    std::string error;
    {
        std::lock_guard<std::mutex> lk(rec->mutex);
        rec->state = ok ? JobState::Succeeded : JobState::Failed;
        error = rec->error;
    }

    // Retire it BEFORE the callback: onFinished is allowed to submit another
    // job (a chained load), and the overlay reads SnapshotActive from the same
    // thread -- neither should ever observe a finished job as still active.
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_active.erase(std::remove(m_active.begin(), m_active.end(), rec), m_active.end());
    }

    // Main thread, so Console and Notify are both legal here. GL is not -- see
    // the note on JobMainStepFn.
    if (rec->desc.onFinished) rec->desc.onFinished(ok, error);
}

// ─────────────────────────────────────────────────────────────────────────────
bool JobSystem::Busy() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return !m_active.empty();
}

bool JobSystem::HasActiveModalJob() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (const auto& rec : m_active)
        if (rec->desc.modal) return true;
    return false;
}

std::vector<JobStatus> JobSystem::SnapshotActive() const {
    std::vector<std::shared_ptr<JobRecord>> live;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        live = m_active;
    }

    std::vector<JobStatus> out;
    out.reserve(live.size());
    for (const auto& rec : live) {
        JobStatus s;
        s.id      = rec->id;
        s.title   = rec->desc.title;
        s.modal   = rec->desc.modal;
        s.graceMs = rec->desc.graceMs;
        {
            std::lock_guard<std::mutex> lk(rec->mutex);
            s.label    = rec->label;
            s.fraction = rec->fraction;
            s.state    = rec->state;
        }
        out.push_back(std::move(s));
    }
    return out;
}
