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
    std::vector<std::pair<class Material*, int>> m_materialShaderSubs;
};
