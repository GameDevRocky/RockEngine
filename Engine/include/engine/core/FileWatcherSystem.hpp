#pragma once
#include "engine/core/System.hpp"
#include <pybind11/embed.h>
#include <string>

namespace py = pybind11;

class FileWatcherSystem : public System {
public:
    static inline const Event FILE_CHANGED_EVENT = Observable::CreateEvent();

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
