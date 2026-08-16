#pragma once
#include "engine/core/System.hpp"
#include <pybind11/embed.h>
#include <string>

namespace py = pybind11;

class FileWatcherSystem : public System {
public:
    // Both carry the changed file's absolute path (normcase'd) as their payload.
    // A rename reports as a DELETED of the old path plus a FILE_CHANGED of the new
    // one, and an atomic save (write temp -> rename over the target) reports a
    // DELETED for the temp file, so a handler that acts on a deletion should
    // confirm against the filesystem rather than trust the event alone.
    static inline const Event FILE_CHANGED_EVENT = Observable::CreateEvent();
    static inline const Event FILE_DELETED_EVENT = Observable::CreateEvent();

    FileWatcherSystem() = default;
    ~FileWatcherSystem() override = default;

    void Init() override;
    void Update() override;
    void Shutdown() override;

    FileWatcherSystem* Copy() override;
    FileWatcherSystem* Copy(Container* container) override;

    void WatchDirectory(const std::string& absPath);

private:
    py::object m_pyWatcher;
};
