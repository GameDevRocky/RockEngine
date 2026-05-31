#include "engine/core/FileWatcherSystem.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <pybind11/gil.h>
#include <pybind11/stl.h>
#include <iostream>
#include <vector>
#include <string>

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
    }
}

void FileWatcherSystem::Update()
{
    if (!m_pyWatcher || m_pyWatcher.is_none()) return;

    std::vector<std::string> changes;
    {
        py::gil_scoped_acquire gil;
        try {
            py::list result = m_pyWatcher.attr("poll_changes")();
            for (auto& item : result) {
                changes.push_back(item.cast<std::string>());
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "[FileWatcherSystem] Python error in Update(): " << e.what() << std::endl;
        }
    }

    for (const auto& path : changes) {
        Notify(FILE_CHANGED_EVENT, path);
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
