#pragma once
#include <functional>
#include <string>

// Progress reporting for the one long operation that happens before there is an
// event loop to report into.
//
// The startup asset load (AssetMetaService::ScanAndGenerate +
// AssetManager::LoadFromDirectory) runs inside the first viewport's
// initializeGL, which Qt triggers from MainWindow::PostInit's showMaximized --
// i.e. before app->exec() ever starts pumping. The job system cannot help there:
// its pump is driven by the frame loop, and the frame loop does not exist yet.
// So this is a plain synchronous callback the loader invokes as it goes, and the
// editor's splash screen answers by force-repainting itself.
//
// Deliberately a std::function rather than an Observable: it is called from deep
// inside the loader on a hot path, it has exactly one listener, and Observable's
// dispatch would drag event-id plumbing into a bootstrap that runs before most
// of the engine exists. Engine stays Qt-free -- the editor installs the sink.
namespace BootProgress {

// fraction in [0,1]; negative means "indeterminate".
using Fn = std::function<void(float fraction, const std::string& label)>;

// Install (or clear, by passing {}) the listener. Main thread, and only around
// the boot sequence -- this is not a general progress channel.
void SetSink(Fn fn);

// No-op when no sink is installed, which is the case for headless or bundled
// runs with no splash.
void Report(float fraction, const std::string& label);

} // namespace BootProgress
