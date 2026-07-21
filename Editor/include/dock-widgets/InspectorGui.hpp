#pragma once
#include <QWidget>
#include <QLayout>
#include <QLayoutItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
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

private:
    void SubscribeToSelector();
    void OnObjectSelected(const std::string& id);
    ~InspectorGui() override = default;
    
    QScrollArea* scrollArea = nullptr;
    QWidget* contentWidget = nullptr;
    ObjectHeader* header = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    Proxy<SelectionManager> selectionManager;
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
};
