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
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include "engine/components/Component.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/commands/ComponentCommand.hpp"
#include "engine/commands/PropertyCommand.hpp"
#include "Engine.hpp"
#include "utils/DragDropMime.hpp"

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
    // The label is the broad, non-interactive part of a component header. It is
    // also the drag handle used to attach this live component to AI chat.
    this->label->installEventFilter(this);
    this->label->setToolTip(QStringLiteral("Drag to attach this component to AI chat"));
}

bool ComponentHeader::eventFilter(QObject* watched, QEvent* event) {
    if (watched != label || component_id.empty())
        return CollapsableWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_dragStart = mouse->position().toPoint();
            m_dragArmed = true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->buttons().testFlag(Qt::LeftButton) &&
            m_dragArmed &&
            (mouse->position().toPoint() - m_dragStart).manhattanLength() >=
                QApplication::startDragDistance()) {
            auto* mime = new QMimeData();
            mime->setData(kComponentMimeType,
                          QString::fromStdString(component_id).toUtf8());
            auto* drag = new QDrag(this);
            drag->setMimeData(mime);
            m_dragStart = {};
            m_dragArmed = false;
            drag->exec(Qt::CopyAction, Qt::CopyAction);
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_dragStart = {};
        m_dragArmed = false;
    }

    return CollapsableWidget::eventFilter(watched, event);
}

void ComponentHeader::OnActiveToggled(bool val){
    if (this->component_id.empty()) return;
    auto* comp = Registry::FindInRuntime<Component>(this->component_id);
    if (!comp) return;
    if (comp->GetEnabled() == val) return;

    comp->SetEnabled(val);

    // No coalescing needed: toggled fires once per completed interaction.
    Container* active = Engine::Get()->GetActiveContainer();
    if (auto* undoSystem = active ? active->FindSystem<UndoSystem>() : nullptr) {
        undoSystem->Push(std::make_unique<PropertyCommand<Component, bool>>(
            component_id, "enabled", !val, val,
            [](Component* c, const bool& v){ bool b = v; c->SetEnabled(b); },
            std::string(val ? "Enable " : "Disable ") + comp->GetTypeName()));
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
    GameObject* go = comp->GetGameObject();
    if (!go) return;

    // RemoveAndRecord does the removal itself so it can snapshot first; the
    // component is removed either way, recorded only if there is a history to
    // record into.
    Container* active = Engine::Get()->GetActiveContainer();
    auto* undoSystem = active ? active->FindSystem<UndoSystem>() : nullptr;
    if (undoSystem) undoSystem->Push(ComponentCommand::RemoveAndRecord(go, comp));
    else            go->RemoveComponent(comp);
}
