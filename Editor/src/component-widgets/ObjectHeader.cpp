#include "component-widgets/ObjectHeader.hpp"
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QPointer>
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/commands/PropertyCommand.hpp"
#include "Engine.hpp"
#include <QHeaderView>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>


ObjectHeader::ObjectHeader(QWidget* parent) 
    : ObjectHeader("Untitled Label", parent) 
{
}

ObjectHeader::ObjectHeader(std::string label, QWidget* parent) 
    : CollapsableWidget(label, parent) 
{
    this->label->setReadOnly(false);
    this->activeButton->setEnabled(true);
}
void ObjectHeader::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ObjectHeader::Bind(const std::string id) {
    QPointer<ObjectHeader> safeThis = this;
    auto* obj = Registry::FindInRuntime<GameObject>(id);
    if (!obj) {
        deleteLater();
        return;
    }
    
    this->gameobject_id = id;
    label->setText(QString::fromStdString(obj->GetName()));
    activeButton->setChecked(obj->GetActive());

    // Both handlers record into the active container's history, so an edit made
    // during play mode is recorded against the runtime world and discarded with it.
    // Neither needs coalescing: toggled and editingFinished each fire once per
    // completed interaction, unlike the spinbox valueChanged stream.
    auto activeUndoSystem = []() -> UndoSystem* {
        Container* active = Engine::Get()->GetActiveContainer();
        return active ? active->FindSystem<UndoSystem>() : nullptr;
    };

    connect(activeButton, &QRadioButton::toggled, safeThis, [safeThis, activeUndoSystem](bool val){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return;
        const bool oldVal = currentObj->GetActive();
        if (oldVal == val) return;

        currentObj->SetActive(val);

        if (auto* undoSystem = activeUndoSystem()) {
            undoSystem->Push(std::make_unique<PropertyCommand<GameObject, bool>>(
                safeThis->gameobject_id, "active", oldVal, val,
                [](GameObject* go, const bool& v){ bool b = v; go->SetActive(b); },
                val ? "Enable Object" : "Disable Object"));
        }
    });

    connect(label, &QLineEdit::editingFinished, safeThis, [safeThis, activeUndoSystem](){
        if (!safeThis) return;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return;
        const std::string oldName = currentObj->GetName();
        const std::string newName = safeThis->label->text().toStdString();
        if (oldName == newName) return;

        currentObj->SetName(newName);

        if (auto* undoSystem = activeUndoSystem()) {
            undoSystem->Push(std::make_unique<PropertyCommand<GameObject, std::string>>(
                safeThis->gameobject_id, "name", oldName, newName,
                [](GameObject* go, const std::string& v){ go->SetName(v); },
                "Rename Object"));
        }
    });

    int nameSub = obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return false;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return false;
        std::string name = std::any_cast<std::string>(data);
        safeThis->label->setText(QString::fromStdString(name));
        return true;
    }, GameObject::NAME_CHANGED_EVENT);

    int activeSub = obj->Subscribe([safeThis](std::any data){
        if (!safeThis) return false;
        auto* currentObj = Registry::FindInRuntime<GameObject>(safeThis->gameobject_id);
        if (!currentObj) return false;
        bool val = std::any_cast<bool>(data);
        safeThis->activeButton->setChecked(val);
        return true;
    }, GameObject::ACTIVE_CHANGED_EVENT);

    m_subs.emplace_back(obj, nameSub);
    m_subs.emplace_back(obj, activeSub);
}