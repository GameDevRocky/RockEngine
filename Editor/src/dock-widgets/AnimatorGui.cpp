#include "dock-widgets/AnimatorGui.hpp"
#include "dock-widgets/AnimatorGraphCanvas.hpp"
#include "utils/AssetPickerWidget.hpp"
#include "utils/AssetThumbnails.hpp"
#include "utils/FrameListWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QMenu>
#include <iostream>
#include "engine/core/Container.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/RuntimeObject.hpp"
#include "engine/components/Animator.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"

namespace {
    void clearLayout(QLayout* layout) {
        if (!layout) return;
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (QWidget* w = item->widget()) w->deleteLater();
            else if (QLayout* cl = item->layout()) clearLayout(cl);
            delete item;
        }
        // Row stretches are a property of the grid, not of the items, so they
        // survive removal — a stale trailing stretch would otherwise keep pushing
        // the next panel's rows around after a rebuild.
        if (auto* grid = qobject_cast<QGridLayout*>(layout))
            for (int r = 0; r < grid->rowCount(); ++r)
                grid->setRowStretch(r, 0);
    }
}

// ─── construction ─────────────────────────────────────────────────────────────

AnimatorGui::AnimatorGui(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    m_emptyPage = new QWidget(this);
    auto* el = new QVBoxLayout(m_emptyPage);
    auto* lbl = new QLabel("No Animator Selected", m_emptyPage);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color:#808080; font-size:18px;");
    el->addStretch(); el->addWidget(lbl); el->addStretch();
    m_stack->addWidget(m_emptyPage);

    BuildEditPage();
    m_stack->addWidget(m_editPage);

    m_stack->setCurrentWidget(m_emptyPage);
}

void AnimatorGui::BuildEditPage() {
    m_editPage = new QWidget(this);
    auto* v = new QVBoxLayout(m_editPage);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(4);

    m_editHeader = new QLabel("Animator", m_editPage);
    m_editHeader->setStyleSheet("color:#e0e0e0; font-size:15px; font-weight:bold;");
    v->addWidget(m_editHeader);

    auto* splitter = new QSplitter(Qt::Horizontal, m_editPage);

    // Left: parameters. Grid, not a box, so a parameter's name and its value line
    // up in the same two columns the object Inspector uses.
    auto* paramsScroll = new QScrollArea();
    paramsScroll->setWidgetResizable(true);
    paramsScroll->setMinimumWidth(170);
    auto* paramsContainer = new QWidget();
    m_paramsLayout = new QGridLayout(paramsContainer);
    m_paramsLayout->setContentsMargins(6, 6, 6, 6);
    m_paramsLayout->setHorizontalSpacing(6);
    m_paramsLayout->setVerticalSpacing(4);
    m_paramsLayout->setColumnStretch(0, 1);
    m_paramsLayout->setColumnStretch(1, 1);
    paramsScroll->setWidget(paramsContainer);
    splitter->addWidget(paramsScroll);

    // Center: node canvas.
    m_canvas = new AnimatorGraphCanvas();
    connect(m_canvas, &AnimatorGraphCanvas::stateSelected, this, [this](const QString& n){ ShowStateInspector(n.toStdString()); });
    connect(m_canvas, &AnimatorGraphCanvas::transitionSelected, this, [this](const QString& id){ ShowTransitionInspector(id.toStdString()); });
    connect(m_canvas, &AnimatorGraphCanvas::selectionCleared, this, [this](){ ClearInspector(); });
    splitter->addWidget(m_canvas);

    // Right: selection inspector. Same two-column grid as the object Inspector.
    auto* inspScroll = new QScrollArea();
    inspScroll->setWidgetResizable(true);
    inspScroll->setMinimumWidth(240);
    auto* inspContainer = new QWidget();
    m_inspectorLayout = new QGridLayout(inspContainer);
    m_inspectorLayout->setContentsMargins(6, 6, 6, 6);
    m_inspectorLayout->setHorizontalSpacing(6);
    m_inspectorLayout->setVerticalSpacing(4);
    m_inspectorLayout->setColumnStretch(0, 1);
    m_inspectorLayout->setColumnStretch(1, 1);
    inspScroll->setWidget(inspContainer);
    splitter->addWidget(inspScroll);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({190, 600, 280});
    v->addWidget(splitter, 1);

    ClearInspector();
}

// ─── row helpers (match the object Inspector's grid) ──────────────────────────

void AnimatorGui::AddRow(QGridLayout* grid, int& row, const std::string& label, QWidget* widget) {
    auto* lbl = new QLabel(QString::fromStdString(label));
    auto font = lbl->font();
    font.setBold(true);
    lbl->setFont(font);
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(lbl, row, 0, Qt::AlignLeft);
    grid->addWidget(widget, row, 1);
    ++row;
}

void AnimatorGui::AddFullRow(QGridLayout* grid, int& row, QWidget* widget) {
    grid->addWidget(widget, row, 0, 1, 2);
    ++row;
}

void AnimatorGui::AddSectionTitle(QGridLayout* grid, int& row, const QString& text, bool spaceAbove) {
    auto* title = new QLabel(text);
    title->setStyleSheet(QString("font-weight:bold; color:#d0d0d0; padding-top:%1px;")
                             .arg(spaceAbove ? 6 : 0));
    AddFullRow(grid, row, title);
}

void AnimatorGui::AddTrailingStretch(QGridLayout* grid, int& row) {
    // A zero-height spacer row that owns all the slack: without it the grid shares
    // extra height between every row and the panel looks loosely spaced.
    grid->setRowStretch(row, 1);
    ++row;
}

// ─── lifecycle / selection following ──────────────────────────────────────────

void AnimatorGui::Init() {
    auto* engine = Engine::Get();
    engine->Subscribe([this]() { SubscribeToSelector(); return true; }, Engine::ENTER_PLAY_MODE_EVENT);
    engine->Subscribe([this]() { UpdateForSelection();  return true; }, Engine::EXIT_PLAY_MODE_EVENT);
    SubscribeToSelector();
    std::cout << "AnimatorGui Initialized" << std::endl;
}

void AnimatorGui::SubscribeToSelector() {
    selectionManager->Subscribe([this](std::any) { UpdateForSelection(); return true; },
                                SelectionManager::SELECTION_CHANGED_EVENT);
    UpdateForSelection();
}

void AnimatorGui::SyncToSelection() { UpdateForSelection(); }

bool AnimatorGui::IsPlayMode() const {
    Container* c = Engine::Get()->GetActiveContainer();
    return c && c->GetMode() == Container::Mode::Runtime;
}

void AnimatorGui::ClearAnimatorSubscriptions() {
    if (!m_animatorSubs.empty()) {
        Container* active = Engine::Get()->GetActiveContainer();
        Registry* reg = active ? active->FindSystem<Registry>() : nullptr;
        if (Animator* a = reg ? reg->Find<Animator>(m_animatorId) : nullptr)
            for (int id : m_animatorSubs) a->Unsubscribe(id);
    }
    m_animatorSubs.clear();
    m_animatorId.clear();
}

void AnimatorGui::UpdateForSelection() {
    ClearAnimatorSubscriptions();

    GameObject* go = dynamic_cast<GameObject*>(selectionManager->GetSerializable());
    Animator* anim = go ? go->GetComponent<Animator>() : nullptr;
    m_animator = anim;

    if (!anim) {
        if (m_canvas) m_canvas->SetAnimator(nullptr);
        m_stack->setCurrentWidget(m_emptyPage);
        return;
    }

    m_animatorId = anim->GetID();

    // Shutdown -> back to the empty state.
    m_animatorSubs.push_back(anim->Subscribe([this]() {
        m_animator = nullptr;
        if (m_canvas) m_canvas->SetAnimator(nullptr);
        m_stack->setCurrentWidget(m_emptyPage);
        return false;   // one-shot
    }, RuntimeObject::SHUTDOWN_EVENT));

    // External structural / parameter / state changes -> refresh the relevant UI.
    m_animatorSubs.push_back(anim->Subscribe([this]() {
        if (m_canvas) m_canvas->update();
        return true;
    }, Animator::GRAPH_CHANGED_EVENT));
    m_animatorSubs.push_back(anim->Subscribe([this]() {
        QMetaObject::invokeMethod(this, [this]{ RebuildParametersPanel(); if (m_canvas) m_canvas->update(); }, Qt::QueuedConnection);
        return true;
    }, Animator::PARAMETERS_CHANGED_EVENT));
    m_animatorSubs.push_back(anim->Subscribe([this]() {
        if (m_canvas) m_canvas->update();   // highlight the live current state in play
        return true;
    }, Animator::STATE_CHANGED_EVENT));

    m_editHeader->setText(QString::fromStdString("Animator — " + (go ? go->GetName() : std::string())));
    if (m_canvas) { m_canvas->SetAnimator(anim); m_canvas->SetEditable(!IsPlayMode()); }
    RebuildParametersPanel();
    ClearInspector();
    m_stack->setCurrentWidget(m_editPage);
}

// ─── parameters panel ─────────────────────────────────────────────────────────

void AnimatorGui::RebuildParametersPanel() {
    if (!m_paramsLayout) return;
    clearLayout(m_paramsLayout);
    m_paramsRow = 0;
    if (!m_animator) return;

    AddSectionTitle(m_paramsLayout, m_paramsRow, "Parameters", false);

    for (const auto& p : m_animator->GetParameters()) {
        const std::string name = p.name;
        const AnimatorParameter::Type type = p.type;
        const float def = p.defaultValue;

        // A parameter's name is itself editable, so the left column is a line edit
        // rather than a static label — the columns still line up with every other
        // row in the panel.
        auto* nameEdit = new QLineEdit(QString::fromStdString(name));
        nameEdit->setToolTip(type == AnimatorParameter::Type::Float   ? "Float"
                           : type == AnimatorParameter::Type::Int     ? "Int"
                           : type == AnimatorParameter::Type::Bool    ? "Bool" : "Trigger");
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, name, nameEdit]() {
            const std::string nn = nameEdit->text().toStdString();
            if (m_animator && !nn.empty() && nn != name) m_animator->RenameParameter(name, nn);
        });

        auto* valueCell = new QWidget();
        auto* rl = new QHBoxLayout(valueCell);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);

        if (type == AnimatorParameter::Type::Float || type == AnimatorParameter::Type::Int) {
            auto* spin = new QDoubleSpinBox();
            spin->setRange(-1e6, 1e6);
            spin->setDecimals(type == AnimatorParameter::Type::Int ? 0 : 3);
            spin->setValue(def);
            spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, name](double v) {
                if (m_animator) if (auto* pp = m_animator->FindParameter(name)) pp->defaultValue = static_cast<float>(v);
            });
            rl->addWidget(spin, 1);
        } else {
            auto* chk = new QCheckBox();
            chk->setChecked(def != 0.0f);
            connect(chk, &QCheckBox::toggled, this, [this, name](bool on) {
                if (m_animator) if (auto* pp = m_animator->FindParameter(name)) pp->defaultValue = on ? 1.0f : 0.0f;
            });
            rl->addWidget(chk, 1);
        }

        auto* del = new QPushButton("×");   // ×
        del->setFixedWidth(22);
        connect(del, &QPushButton::clicked, this, [this, name]() { if (m_animator) m_animator->RemoveParameter(name); });
        rl->addWidget(del);

        m_paramsLayout->addWidget(nameEdit, m_paramsRow, 0);
        m_paramsLayout->addWidget(valueCell, m_paramsRow, 1);
        ++m_paramsRow;
    }

    auto* addBtn = new QPushButton("+ Parameter");
    connect(addBtn, &QPushButton::clicked, this, [this, addBtn]() {
        QMenu menu(addBtn);
        menu.addAction("Float",   this, [this]{ if (m_animator) m_animator->AddParameter("New Float",   AnimatorParameter::Type::Float); });
        menu.addAction("Int",     this, [this]{ if (m_animator) m_animator->AddParameter("New Int",     AnimatorParameter::Type::Int); });
        menu.addAction("Bool",    this, [this]{ if (m_animator) m_animator->AddParameter("New Bool",    AnimatorParameter::Type::Bool); });
        menu.addAction("Trigger", this, [this]{ if (m_animator) m_animator->AddParameter("New Trigger", AnimatorParameter::Type::Trigger); });
        menu.exec(addBtn->mapToGlobal(QPoint(0, addBtn->height())));
    });
    AddFullRow(m_paramsLayout, m_paramsRow, addBtn);
    AddTrailingStretch(m_paramsLayout, m_paramsRow);
}

// ─── selection inspector ──────────────────────────────────────────────────────

void AnimatorGui::ClearInspector() {
    m_inspectorState.clear();
    m_inspectorTransitionId.clear();
    if (!m_inspectorLayout) return;
    clearLayout(m_inspectorLayout);
    m_inspectorRow = 0;
    m_frameList = nullptr;
    auto* hint = new QLabel("Select a state or transition");
    hint->setStyleSheet("color:#888;");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    AddFullRow(m_inspectorLayout, m_inspectorRow, hint);
    AddTrailingStretch(m_inspectorLayout, m_inspectorRow);
}

void AnimatorGui::ShowStateInspector(const std::string& stateName) {
    m_inspectorState = stateName;
    m_inspectorTransitionId.clear();
    clearLayout(m_inspectorLayout);
    m_inspectorRow = 0;
    m_frameList = nullptr;
    if (!m_animator) return;
    AnimatorState* s = m_animator->FindState(stateName);
    if (!s) { m_inspectorState.clear(); return; }

    AddSectionTitle(m_inspectorLayout, m_inspectorRow, "State", false);

    auto* nameEdit = new QLineEdit(QString::fromStdString(s->name));
    connect(nameEdit, &QLineEdit::editingFinished, this, [this, stateName, nameEdit]() {
        const std::string nn = nameEdit->text().toStdString();
        if (!m_animator || nn.empty() || nn == stateName) return;
        m_animator->RenameState(stateName, nn);
        if (m_canvas) m_canvas->SelectState(nn);
        QMetaObject::invokeMethod(this, [this, nn]{ ShowStateInspector(nn); }, Qt::QueuedConnection);
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Name: ", nameEdit);

    auto* fps = new QDoubleSpinBox();
    fps->setRange(0.0, 240.0);
    fps->setValue(s->frameRate);
    fps->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    connect(fps, &QDoubleSpinBox::valueChanged, this, [this, stateName](double v) {
        if (m_animator) if (auto* st = m_animator->FindState(stateName)) st->frameRate = static_cast<float>(v);
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Frame Rate: ", fps);

    auto* speed = new QDoubleSpinBox();
    speed->setRange(0.0, 100.0);
    speed->setSingleStep(0.1);
    speed->setValue(s->speed);
    speed->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    connect(speed, &QDoubleSpinBox::valueChanged, this, [this, stateName](double v) {
        if (m_animator) if (auto* st = m_animator->FindState(stateName)) st->speed = static_cast<float>(v);
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Speed: ", speed);

    auto* loop = new QCheckBox();
    loop->setChecked(s->loop);
    connect(loop, &QCheckBox::toggled, this, [this, stateName](bool on) {
        if (m_animator) if (auto* st = m_animator->FindState(stateName)) st->loop = on;
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Loop: ", loop);

    // Frames — the one part that earns the full panel width, since each row is
    // itself a label+field pair (the sprite reference widget).
    AddSectionTitle(m_inspectorLayout, m_inspectorRow, "Frames");

    auto* frames = new FrameListWidget();
    m_frameList = frames;
    frames->getFrames = [this, stateName]() -> std::vector<std::string> {
        if (!m_animator) return {};
        AnimatorState* st = m_animator->FindState(stateName);
        return st ? st->frames : std::vector<std::string>{};
    };
    frames->onSetFrame = [this, stateName](int idx, const std::string& id) {
        // In-place edit: no structural change, so no rebuild — rebuilding here
        // would delete the very widget handling this callback.
        if (m_animator) m_animator->SetFrame(stateName, idx, id);
    };
    frames->onAddFrames = [this, stateName](const std::vector<std::string>& ids) {
        if (!m_animator) return;
        for (const auto& id : ids) m_animator->AddFrame(stateName, id);
        RefreshFrameList();
    };
    frames->onRemoveFrame = [this, stateName](int idx) {
        if (m_animator) m_animator->RemoveFrame(stateName, idx);
        RefreshFrameList();
    };
    frames->onMoveFrame = [this, stateName](int from, int to) {
        if (m_animator) m_animator->MoveFrame(stateName, from, to);
        RefreshFrameList();
    };
    frames->Rebuild();
    AddFullRow(m_inspectorLayout, m_inspectorRow, frames);

    AddTrailingStretch(m_inspectorLayout, m_inspectorRow);
}

// Frame edits are triggered from inside the frame rows themselves, so the rebuild
// has to be queued — tearing the rows down underneath the click handler that asked
// for it would destroy the sender mid-signal.
void AnimatorGui::RefreshFrameList() {
    QMetaObject::invokeMethod(this, [this]() {
        if (m_frameList) m_frameList->Rebuild();
    }, Qt::QueuedConnection);
}

void AnimatorGui::ShowTransitionInspector(const std::string& transitionId) {
    m_inspectorTransitionId = transitionId;
    m_inspectorState.clear();
    clearLayout(m_inspectorLayout);
    m_inspectorRow = 0;
    m_frameList = nullptr;
    if (!m_animator) return;
    AnimatorTransition* t = m_animator->FindTransition(transitionId);
    if (!t) { m_inspectorTransitionId.clear(); return; }

    const std::string from = t->fromAnyState ? "Any State" : t->fromState;
    AddSectionTitle(m_inspectorLayout, m_inspectorRow, "Transition", false);

    auto* route = new QLabel(QString::fromStdString(from + "  →  " + t->toState));
    route->setStyleSheet("color:#aaa;");
    route->setWordWrap(true);
    AddFullRow(m_inspectorLayout, m_inspectorRow, route);

    auto* het = new QCheckBox();
    het->setChecked(t->hasExitTime);
    connect(het, &QCheckBox::toggled, this, [this, transitionId](bool on) {
        if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId)) tr->hasExitTime = on;
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Has Exit Time: ", het);

    auto* exit = new QDoubleSpinBox();
    exit->setRange(0.0, 1.0);
    exit->setSingleStep(0.05);
    exit->setValue(t->exitTime);
    exit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    connect(exit, &QDoubleSpinBox::valueChanged, this, [this, transitionId](double v) {
        if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId)) tr->exitTime = static_cast<float>(v);
    });
    AddRow(m_inspectorLayout, m_inspectorRow, "Exit Time: ", exit);

    AddSectionTitle(m_inspectorLayout, m_inspectorRow, "Conditions");

    QStringList paramNames;
    for (const auto& p : m_animator->GetParameters()) paramNames << QString::fromStdString(p.name);

    for (int ci = 0; ci < static_cast<int>(t->conditions.size()); ++ci) {
        const AnimatorCondition& c = t->conditions[ci];
        auto* row = new QWidget();
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(2);

        auto* paramCombo = new QComboBox();
        paramCombo->addItems(paramNames);
        paramCombo->setCurrentText(QString::fromStdString(c.parameter));
        connect(paramCombo, &QComboBox::currentTextChanged, this, [this, transitionId, ci](const QString& txt) {
            if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId))
                if (ci < static_cast<int>(tr->conditions.size())) tr->conditions[ci].parameter = txt.toStdString();
        });
        rl->addWidget(paramCombo, 1);

        auto* modeCombo = new QComboBox();
        modeCombo->addItems({ ">", "<", "==", "!=", "true", "false" });
        modeCombo->setCurrentIndex(static_cast<int>(c.mode));
        connect(modeCombo, &QComboBox::currentIndexChanged, this, [this, transitionId, ci](int idx) {
            if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId))
                if (ci < static_cast<int>(tr->conditions.size())) tr->conditions[ci].mode = static_cast<AnimatorCondition::Mode>(idx);
        });
        rl->addWidget(modeCombo);

        auto* thr = new QDoubleSpinBox();
        thr->setRange(-1e6, 1e6);
        thr->setValue(c.threshold);
        thr->setFixedWidth(60);
        connect(thr, &QDoubleSpinBox::valueChanged, this, [this, transitionId, ci](double v) {
            if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId))
                if (ci < static_cast<int>(tr->conditions.size())) tr->conditions[ci].threshold = static_cast<float>(v);
        });
        rl->addWidget(thr);

        auto* del = new QPushButton("×");
        del->setFixedWidth(22);
        connect(del, &QPushButton::clicked, this, [this, transitionId, ci]() {
            if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId))
                if (ci < static_cast<int>(tr->conditions.size())) tr->conditions.erase(tr->conditions.begin() + ci);
            QMetaObject::invokeMethod(this, [this, transitionId]{ ShowTransitionInspector(transitionId); }, Qt::QueuedConnection);
        });
        rl->addWidget(del);

        // A condition is one expression, not a name/value pair, so it spans both
        // columns rather than being squeezed into the right-hand one.
        AddFullRow(m_inspectorLayout, m_inspectorRow, row);
    }

    auto* addCond = new QPushButton("Add Condition");
    connect(addCond, &QPushButton::clicked, this, [this, transitionId]() {
        if (m_animator) if (auto* tr = m_animator->FindTransition(transitionId)) {
            AnimatorCondition c;
            if (!m_animator->GetParameters().empty()) c.parameter = m_animator->GetParameters().front().name;
            tr->conditions.push_back(c);
        }
        QMetaObject::invokeMethod(this, [this, transitionId]{ ShowTransitionInspector(transitionId); }, Qt::QueuedConnection);
    });
    AddFullRow(m_inspectorLayout, m_inspectorRow, addCond);

    auto* delT = new QPushButton("Delete Transition");
    connect(delT, &QPushButton::clicked, this, [this, transitionId]() {
        if (m_animator) m_animator->RemoveTransition(transitionId);
        if (m_canvas) m_canvas->ClearSelection();
        QMetaObject::invokeMethod(this, [this]{ ClearInspector(); }, Qt::QueuedConnection);
    });
    AddFullRow(m_inspectorLayout, m_inspectorRow, delT);

    AddTrailingStretch(m_inspectorLayout, m_inspectorRow);
}
