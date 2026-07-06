#include "component-widgets/ComponentHeader.hpp"
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QPointer>
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include <QHeaderView>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include "engine/components/Component.hpp"

ComponentHeader::ComponentHeader(QWidget* parent)
    : ComponentHeader("Untitled Label", parent)
{
}

ComponentHeader::ComponentHeader(std::string label, QWidget* parent)
    : CollapsableWidget(label, parent)
{
    this->activeButton->setEnabled(true);
    // The label is a read-only QLineEdit; without this it eats right-clicks with its
    // own copy/paste menu. Defer to us so the delete menu works over the whole header.
    this->label->setContextMenuPolicy(Qt::NoContextMenu);
}

void ComponentHeader::OnActiveToggled(bool val){
    if (this->component_id.empty()) return;
    auto* comp = Registry::FindInRuntime<Component>(this->component_id);
    if (comp) {
        comp->SetEnabled(val); 
    }

}

void ComponentHeader::Bind(std::string id){
    QPointer<ComponentHeader> safeThis = this;
    Component* comp = Registry::FindInRuntime<Component>(id);
    if (!comp) {
        deleteLater();
        return;
    }
    this->component_id = comp->GetID();
    label->setText(QString::fromStdString(comp->GetTypeName()));
    activeButton->setChecked(comp->GetEnabled());

    int enabledSub = comp->Subscribe([safeThis](std::any data){
        if (!safeThis) return false;
        auto* comp = Registry::FindInRuntime<Component>(safeThis->component_id);
        if (!comp) return false;
        bool val = std::any_cast<bool>(data);
        safeThis->activeButton->setChecked(val);
        return true;
    }, Component::ENABLED_CHANGED_EVENT);

    m_subs.emplace_back(comp, enabledSub);
}

void ComponentHeader::paintEvent(QPaintEvent *event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ComponentHeader::contextMenuEvent(QContextMenuEvent* event) {
    if (component_id.empty()) return;

    QMenu menu(this);
    QAction* deleteAction = menu.addAction("Delete Component");

    // Every GameObject needs a Transform, so its removal is blocked.
    if (Component* comp = Registry::FindInRuntime<Component>(component_id)) {
        if (comp->GetTypeName() == "Transform")
            deleteAction->setEnabled(false);
    }

    if (menu.exec(event->globalPos()) != deleteAction) return;

    // Re-resolve after the modal menu closed, then hand off to the owning object.
    // RemoveComponent fires REMOVE_COMPONENT_EVENT, which rebuilds the inspector and
    // deleteLater()s this widget — so don't touch `this` afterwards.
    Component* comp = Registry::FindInRuntime<Component>(component_id);
    if (!comp) return;
    if (GameObject* go = comp->GetGameObject())
        go->RemoveComponent(comp);
}