#pragma once
#include <QWidget>
#include <QLayout>
#include <QLayoutItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include "component-widgets/ObjectHeader.hpp"
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"

class Observable;

using namespace EngineUtils;

class InspectorGui : public QWidget {
    Q_OBJECT

public:
    static InspectorGui* Get() {
        static InspectorGui* instance = nullptr;
        if (!instance) {
            instance = new InspectorGui(nullptr);
        }
        return instance;
    }
    void Init();
    explicit InspectorGui(QWidget* parent = nullptr);

    // Rebuild the inspector for whatever is currently selected. For components
    // whose set of visible rows depends on one of their own fields (e.g. a
    // Light's Type dropdown decides whether the spot-cone rows apply), the
    // property visitor calls this from the field's change event.
    //
    // Always deferred through the event loop: the caller is typically inside a
    // widget's own signal handler, and rebuilding synchronously would delete
    // that widget mid-callback.
    void RequestRebuild();

private:
    void SubscribeToSelector();
    void OnObjectSelected(const std::string& id);
    // Add a ScriptComponent running the given "module:class" script to the
    // currently-selected GameObject (drag-drop of a .py from the Folder view onto
    // the inspector). No-op unless the selection is a GameObject.
    void AddScriptToSelectedObject(const std::string& scriptRef);
    // Flush any property widgets flagged dirty by an engine change event since the
    // last tick. Driven by m_pollTimer so live values (e.g. a moving object's
    // position) refresh at ~20 Hz instead of every engine frame. Runs in all modes.
    void RefreshDirtyValues();
    // Poll the currently-inspected ScriptComponent(s) for Python-side field
    // mutations. Python scripts change fields by plain attribute assignment, which
    // fires no change event, so without this the inspector widgets go stale during
    // play. Driven by m_pollTimer; a no-op outside Runtime mode (scripts don't run).
    void PollScriptFields();
    ~InspectorGui() override = default;
    
    QScrollArea* scrollArea = nullptr;
    QWidget* contentWidget = nullptr;
    ObjectHeader* header = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    Proxy<SelectionManager> selectionManager;
    QTimer* m_pollTimer = nullptr;
    // Component ids of the ScriptComponents in the current inspection, polled by
    // m_pollTimer for Python-side field changes. Rebuilt each OnObjectSelected.
    std::vector<std::string> m_polledScriptIds;
    std::vector<std::pair<std::string, int>> m_scriptReloadSubs;
    // Store the material's asset id (not a raw Material*): an asset can be deleted
    // (e.g. removed on disk) while its subscription handle is still held here, which
    // would leave a dangling pointer. Re-resolve through AssetManager before
    // unsubscribing so a since-deleted material is simply skipped.
    std::vector<std::pair<std::string, int>> m_materialShaderSubs;
    // Per-rebuild subscriptions from BindProperty and the header Bind() calls,
    // torn down at the top of OnObjectSelected. Targets are alive at teardown:
    // object deletion is deferred (Registry::FlushPendingShutdowns shuts down —
    // firing the deselect that triggers teardown — then deletes), and assets live
    // for the whole session.
    std::vector<std::pair<Observable*, int>> m_inspectorSubs;
    // Id currently shown in the inspector. When OnObjectSelected rebuilds for the
    // SAME id (add/remove component, script hot-reload), the scroll position is
    // preserved so the rebuild isn't jarring; a different id resets to the top.
    std::string m_currentInspectedId;
    // Deferred value-refresh closures collected from the property visitors for the
    // current selection, flushed by RefreshDirtyValues on the m_pollTimer tick.
    // Cleared/rebuilt in OnObjectSelected alongside m_inspectorSubs.
    std::vector<std::function<void()>> m_valueRefreshers;
};
