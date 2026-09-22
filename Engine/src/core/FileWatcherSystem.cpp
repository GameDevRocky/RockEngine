#include "engine/core/FileWatcherSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <pybind11/gil.h>
#include <pybind11/stl.h>
#include <iostream>
#include <vector>
#include <string>
#include <utility>

using namespace EngineUtils;
namespace py = pybind11;

void FileWatcherSystem::Init()
{
    py::gil_scoped_acquire gil;
    try {
        py::module mod = py::module::import("Domain.lib.utils.file_watcher");
        m_pyWatcher = mod.attr("FileWatcher").attr("get")();
        std::string scriptsPath = GetAssetPath("Domain/sandbox/scripts");
        m_pyWatcher.attr("watch_directory")(scriptsPath);
    } catch (const py::error_already_set& e) {
        std::cerr << "[FileWatcherSystem] Python error in Init(): " << e.what() << std::endl;
        // A missing watchdog is the one failure with an obvious fix, and the traceback alone
        // does not say which interpreter to install it into -- the embedded one need not be
        // the "python" on PATH. Print the prefix that was actually used.
        if (e.matches(PyExc_ModuleNotFoundError)) {
            try {
                py::module_ sys = py::module_::import("sys");
                std::cerr << "[FileWatcherSystem] Hot reload is off. The embedded interpreter's "
                             "prefix is " << sys.attr("prefix").cast<std::string>()
                          << "; install requirements.txt there (pip install -r requirements.txt)."
                          << std::endl;
            } catch (const py::error_already_set&) {
                // Diagnostics only -- never let them replace the real error above.
            }
        }
    }
}

void FileWatcherSystem::Update()
{
    if (!m_pyWatcher || m_pyWatcher.is_none()) return;

    // (kind, path) pairs, drained under the GIL and dispatched without it — a
    // handler may run arbitrary engine work (a script re-instantiation) that
    // takes the GIL itself.
    std::vector<std::pair<std::string, std::string>> changes;
    {
        py::gil_scoped_acquire gil;
        try {
            py::list result = m_pyWatcher.attr("poll_changes")();
            for (auto& item : result) {
                auto entry = item.cast<py::tuple>();
                changes.emplace_back(entry[0].cast<std::string>(),
                                     entry[1].cast<std::string>());
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "[FileWatcherSystem] Python error in Update(): " << e.what() << std::endl;
        }
    }

    for (const auto& [kind, path] : changes) {
        Notify(kind == "deleted" ? FILE_DELETED_EVENT : FILE_CHANGED_EVENT, path);
    }
}

void FileWatcherSystem::Shutdown()
{
    if (Py_IsInitialized() && m_pyWatcher && !m_pyWatcher.is_none()) {
        py::gil_scoped_acquire gil;
        m_pyWatcher = py::object();
    }
}

void FileWatcherSystem::WatchDirectory(const std::string& absPath)
{
    if (!m_pyWatcher || m_pyWatcher.is_none()) return;
    py::gil_scoped_acquire gil;
    try {
        m_pyWatcher.attr("watch_directory")(absPath);
    } catch (const py::error_already_set& e) {
        std::cerr << "[FileWatcherSystem] Python error in WatchDirectory(): " << e.what() << std::endl;
    }
}

FileWatcherSystem* FileWatcherSystem::Copy()
{
    return new FileWatcherSystem();
}

FileWatcherSystem* FileWatcherSystem::Copy(Container* container)
{
    auto* copy = new FileWatcherSystem();
    copy->Attach(container);
    return copy;
}
