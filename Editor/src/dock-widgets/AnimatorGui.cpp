#include "dock-widgets/AnimatorGui.hpp"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <iostream>
#include "engine/core/Container.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "engine/components/Animator.hpp"

AnimatorGui::AnimatorGui(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    // Page 0 -- nothing to edit.
    m_emptyPage = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    auto* emptyLabel = new QLabel("No Animator Selected", m_emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #808080; font-size: 18px;");
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyPage);

    // Page 1 -- the editor. Placeholder content for now; the real graph/frame
    // editor lands later. The header names the animator being edited.
    m_editPage = new QWidget(this);
    auto* editLayout = new QVBoxLayout(m_editPage);
    editLayout->setContentsMargins(8, 8, 8, 8);
    m_editHeader = new QLabel("Animator", m_editPage);
    m_editHeader->setStyleSheet("color: #e0e0e0; font-size: 16px; font-weight: bold;");
    editLayout->addWidget(m_editHeader);
    auto* placeholder = new QLabel("Animation editor coming soon…", m_editPage);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #666666;");
    editLayout->addWidget(placeholder, 1);
    m_stack->addWidget(m_editPage);

    m_stack->setCurrentWidget(m_emptyPage);
}

void AnimatorGui::Init() {
    auto* engine = Engine::Get();
    // Entering play mode: the runtime container's SelectionManager is a fresh copy
    // with no subscribers, so re-wire onto it (mirrors InspectorGui).
    engine->Subscribe([this]() { SubscribeToSelector(); return true; },
                      Engine::ENTER_PLAY_MODE_EVENT);
    // Exiting play mode: the editor SelectionManager still holds our Init-time
    // subscription, so don't re-subscribe (that would stack duplicates) -- just
    // refresh for the editor's current selection.
    engine->Subscribe([this]() { UpdateForSelection(); return true; },
                      Engine::EXIT_PLAY_MODE_EVENT);
    SubscribeToSelector();
    std::cout << "AnimatorGui Initialized" << std::endl;
}

void AnimatorGui::SubscribeToSelector() {
    selectionManager->Subscribe([this](std::any) {
        UpdateForSelection();
        return true;
    }, SelectionManager::SELECTION_CHANGED_EVENT);
    UpdateForSelection();
}

void AnimatorGui::SyncToSelection() {
    UpdateForSelection();
}

void AnimatorGui::ClearAnimatorSubscription() {
    if (m_animatorShutdownSub == -1) return;
    // Only unsubscribe if that Animator is still alive; if it already shut down,
    // the one-shot callback removed the subscription and cleared the handle.
    Container* active = Engine::Get()->GetActiveContainer();
    Registry* reg = active ? active->FindSystem<Registry>() : nullptr;
    if (reg) {
        if (Animator* prev = reg->Find<Animator>(m_animatorId))
            prev->Unsubscribe(m_animatorShutdownSub);
    }
    m_animatorShutdownSub = -1;
    m_animatorId.clear();
}

void AnimatorGui::UpdateForSelection() {
    ClearAnimatorSubscription();

    GameObject* go = dynamic_cast<GameObject*>(selectionManager->GetSerializable());
    Animator* anim = go ? go->GetComponent<Animator>() : nullptr;

    if (anim) {
        // Watch this Animator's shutdown: if the component is removed or its object
        // deleted (GameObject::Shutdown shuts down its components), drop to empty.
        m_animatorId = anim->GetID();
        m_animatorShutdownSub = anim->Subscribe([this]() {
            m_animatorShutdownSub = -1;   // one-shot
            m_animatorId.clear();
            m_stack->setCurrentWidget(m_emptyPage);
            return false;                 // return false -> unsubscribe after firing
        }, RuntimeObject::SHUTDOWN_EVENT);

        m_editHeader->setText(QString::fromStdString("Animator — " + go->GetName()));
        m_stack->setCurrentWidget(m_editPage);
    } else {
        m_stack->setCurrentWidget(m_emptyPage);
    }
}
