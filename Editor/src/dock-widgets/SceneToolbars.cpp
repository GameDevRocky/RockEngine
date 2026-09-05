#include "dock-widgets/SceneToolbars.hpp"

#include <QCursor>
#include <QEnterEvent>
#include <QGraphicsOpacityEffect>
#include <QSettings>
#include <QComboBox>
#include <QListView>
#include "dock-widgets/SceneViewGui.hpp"

#include "engine/commands/EditorSettingCommand.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/rendering/core/GizmoSettings.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/rendering/core/GridSettings.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/IconMaps.h"

#include <QBoxLayout>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>
#include <algorithm>

namespace {

// Shared by both bars, so one #SceneNativeToolbar rule covers them.
const char* kToolbarStyle =
    "#SceneNativeToolbar {"
    "  background-color: rgba(42, 42, 42, 235);"
    "  border: 1px solid rgb(78, 78, 78);"
    "  border-radius: 4px;"
    "}"
    "#SceneNativeToolbar QToolButton {"
    "  color: white;"
    "  background-color: rgb(51, 51, 51);"
    "  border: 1px solid rgb(72, 72, 72);"
    "  border-radius: 3px;"
    "}"
    "#SceneNativeToolbar QToolButton:hover {"
    "  background-color: rgb(82, 174, 82);"
    "}"
    "#SceneNativeToolbar QToolButton:checked,"
    "#SceneNativeToolbar QToolButton:pressed {"
    "  background-color: rgb(66, 148, 66);"
    "}"
    "#SceneNativeToolbar QToolButton:disabled {"
    "  color: rgb(120, 120, 120);"
    "  background-color: rgb(45, 45, 45);"
    "}"
    "#SceneNativeToolbar QDoubleSpinBox,"
    "#SceneNativeToolbar QSpinBox {"
    "  color: white;"
    "  background-color: rgb(51, 51, 51);"
    "  border: 1px solid rgb(72, 72, 72);"
    "  border-radius: 3px;"
    "  selection-background-color: rgb(66, 148, 66);"
    "}"
    "#SceneNativeToolbar QComboBox {"
    "  color: white;"
    "  background-color: rgb(51, 51, 51);"
    "  border: 1px solid rgb(72, 72, 72);"
    "  border-radius: 3px;"
    "  padding: 2px 6px;"
    "}"
    "#SceneNativeToolbar QComboBox:hover {"
    "  border: 1px solid rgb(102, 102, 102);"
    "}"
    "#SceneNativeToolbar QComboBox::drop-down {"
    "  border: none;"
    "  width: 16px;"
    "}"
    "#SceneToolbarContents {"
    "  background: transparent;"
    "  border: none;"
    "}"
    "#SceneNativeToolbar #SceneToolbarCollapse {"
    "  background: transparent;"
    "  border: 1px solid transparent;"
    "}"
    "#SceneNativeToolbar #SceneToolbarCollapse:hover {"
    "  background-color: rgb(59, 59, 59);"
    "}";

const char* kMenuStyle =
    "QMenu {"
    "  background-color: rgb(42, 42, 42);"
    "  border: 1px solid rgb(78, 78, 78);"
    "  border-radius: 4px;"
    "  color: white;"
    "  padding: 4px;"
    "}"
    "QMenu::item { padding: 4px 24px 4px 24px; border-radius: 3px; }"
    "QMenu::item:selected { background-color: rgb(66, 148, 66); }"
    "QMenu::item:disabled { color: rgb(120, 120, 120); }";

// Set a checkable button without re-entering its clicked handler.
//
// setChecked() emits toggled() but NOT clicked(), and every button here connects
// to clicked -- which is exactly what stops
// "click -> engine setter -> CHANGED_EVENT -> Refresh -> setChecked" from
// looping. Do not switch these connections to toggled().
void SetChecked(QToolButton* button, bool checked) {
    if (button && button->isChecked() != checked) button->setChecked(checked);
}

void SetTransformOperation(ImGuizmo::OPERATION operation) {
    auto* gizmos = GizmosManager::Get();
    gizmos->SetOperation(operation);
    gizmos->SetEditMode(GizmosManager::EditMode::Transform);
}

// Apply an editor-setting change AND record it for undo.
//
// `apply` is the single source of truth for how the value is written: it is used
// for the change now and stored in the command for undo/redo, so the forward and
// reverse paths cannot drift apart.
//
// Recording happens HERE, at the editor call site, never inside the engine
// mutators themselves -- a script or MCP call that pokes GridSettings must not
// silently land on the user's undo stack.
//
// Undoing calls `apply`, which notifies CHANGED_EVENT, which refreshes the
// toolbar. That does not re-record: the widgets are updated with setChecked /
// SetValue, neither of which emits the clicked/onChanged signal these handlers
// hang off, and UndoSystem::Push additionally no-ops while IsApplying().
// Takes the key and label by value rather than as const char*: some call sites
// compose them per category, and a std::string parameter removes any question
// about a temporary's lifetime reaching into the command that outlives the call.
template <typename ValueT>
void EditSetting(std::string settingKey, std::string text,
                 const std::function<ValueT()>& read,
                 std::function<void(const ValueT&)> apply,
                 ValueT next,
                 bool mergeable = false)
{
    const ValueT prev = read();
    if (prev == next) return;

    apply(next);

    Container* active = Engine::Get()->GetActiveContainer();
    // Null in AppMode::Player, which builds no UndoSystem. The setting still
    // applies; only the history is skipped.
    UndoSystem* undo = active ? active->FindSystem<UndoSystem>() : nullptr;
    if (!undo) return;

    undo->Push(std::make_unique<EditorSettingCommand<ValueT>>(
        std::move(settingKey), prev, next, std::move(apply), std::move(text), mergeable));
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// SceneOverlayToolbar
// ═════════════════════════════════════════════════════════════════════════════

SceneOverlayToolbar::SceneOverlayToolbar(QWidget* parent, Qt::Orientation orientation,
                                         QString settingsKey, bool movable)
    : QWidget(parent)
    , m_orientation(orientation)
    , m_settingsKey(std::move(settingsKey))
    , m_movable(movable)
{
    setObjectName(QStringLiteral("SceneNativeToolbar"));

    // Fades the whole subtree -- icons, fields and background together. Applying alpha
    // through the stylesheet instead would have to be repeated for every rule and would
    // miss the QToolButton arrows entirely.
    //
    // Starts faded: the bar has not been pointed at yet, and having it snap from opaque
    // to ghost on the first stray mouse movement looks like a glitch.
    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(kUnfocusedOpacity);
    setGraphicsEffect(m_opacity);
    if (m_movable) setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString::fromUtf8(kToolbarStyle));

    const bool horizontal = IsHorizontal();
    const auto direction  = horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom;
    const auto alignment  = horizontal ? Qt::AlignVCenter : Qt::AlignHCenter;

    auto* layout = new QBoxLayout(direction, this);
    layout->setContentsMargins(6, 5, 6, 6);
    layout->setSpacing(3);
    layout->setAlignment(alignment);
    // Shrink-wrap: the bar is exactly as big as its controls, never stretched.
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // The grab area. Transparent for mouse events so the press falls through to
    // this widget's own handler rather than being eaten by the label.
    // Omitted on a pinned bar, where it would advertise a drag that does nothing.
    if (m_movable) {
        auto* handle = new QLabel(QString::fromUtf8(horizontal ? "⋮" : "•••"), this);
        handle->setAlignment(Qt::AlignCenter);
        if (horizontal) handle->setFixedWidth(10);
        else            handle->setFixedHeight(14);
        handle->setToolTip(QStringLiteral("Drag toolbar"));
        handle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        handle->setStyleSheet("color: rgb(135, 135, 135); background: transparent;");
        layout->addWidget(handle);
    }

    m_collapse = new QToolButton(this);
    m_collapse->setObjectName(QStringLiteral("SceneToolbarCollapse"));
    if (horizontal) m_collapse->setFixedSize(18, kControlHeight);
    else            m_collapse->setFixedSize(kIconWidth, 18);
    m_collapse->setFocusPolicy(Qt::NoFocus);
    m_collapse->setCursor(Qt::PointingHandCursor);
    m_collapse->setArrowType(horizontal ? Qt::LeftArrow : Qt::UpArrow);
    m_collapse->setToolTip(QStringLiteral("Collapse toolbar"));
    connect(m_collapse, &QToolButton::clicked, this, [this] { SetCollapsed(!m_collapsed); });
    layout->addWidget(m_collapse, 0, alignment);

    m_contents = new QWidget(this);
    m_contents->setObjectName(QStringLiteral("SceneToolbarContents"));
    m_contents->setAttribute(Qt::WA_StyledBackground, true);
    m_controlsLayout = new QBoxLayout(direction, m_contents);
    m_controlsLayout->setContentsMargins(0, 0, 0, 0);
    m_controlsLayout->setSpacing(3);
    m_controlsLayout->setAlignment(alignment);
    layout->addWidget(m_contents, 0, alignment);

    const int fontId = QFontDatabase::addApplicationFont(QString::fromStdString(
        EngineUtils::GetAssetPath("Domain/lib/assets/fonts/Font Awesome 7 Free-Solid-900.otf")));
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.empty()) {
            m_iconFont = QFont(families.front());
            m_iconFont.setPixelSize(14);
        }
    }
}

SceneOverlayToolbar::~SceneOverlayToolbar()
{
    // Only long-lived sources go through SubscribeTo, so these are all still
    // alive here. A dangling capture of `this` in one of their callbacks would
    // otherwise fire after the widget is gone.
    for (auto& [source, id] : m_subs)
        if (source) source->Unsubscribe(id);
    m_subs.clear();
}

void SceneOverlayToolbar::SubscribeTo(Observable* source, Observable::Event event,
                                      std::function<void()> handler)
{
    if (!source) return;
    const int id = source->Subscribe([handler = std::move(handler)]() {
        handler();
        return true;   // false would auto-unsubscribe; see the header.
    }, event);
    m_subs.emplace_back(source, id);
}

QToolButton* SceneOverlayToolbar::AddButton(const char* icon, const char* tooltip)
{
    auto* button = new QToolButton(this);
    button->setCheckable(true);
    button->setFixedSize(kIconWidth, kControlHeight);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setFont(m_iconFont);
    button->setText(QString::fromUtf8(icon));
    button->setToolTip(QString::fromUtf8(tooltip));
    m_controlsLayout->addWidget(button, 0, IsHorizontal() ? Qt::AlignVCenter : Qt::AlignHCenter);
    return button;
}


QComboBox* SceneOverlayToolbar::AddComboBox(const char* tooltip, int width)
{
    auto* combo = new QComboBox(m_contents);
    combo->setFocusPolicy(Qt::NoFocus);   // same as every other control here: the
                                          // viewport keeps keyboard focus
    combo->setCursor(Qt::PointingHandCursor);
    combo->setFixedHeight(kControlHeight);
    if (width > 0) combo->setFixedWidth(width);
    if (tooltip)   combo->setToolTip(QString::fromUtf8(tooltip));

    // The popup is a top-level window, NOT a child of #SceneNativeToolbar, so the
    // stylesheet's descendant rules never reach it and it would render in the default
    // palette -- a bright list hanging off a dark bar. Styling the view directly is what
    // fixes that, and kMenuStyle is reused so the popup matches the toolbar's own menus.
    auto* view = new QListView(combo);
    view->setStyleSheet(QString::fromUtf8(kMenuStyle).replace("QMenu", "QListView")
                        + "QListView::item { padding: 4px 8px; }"
                          "QListView::item:selected { background-color: rgb(66, 148, 66); }");
    combo->setView(view);

    m_controlsLayout->addWidget(combo, 0, IsHorizontal() ? Qt::AlignVCenter : Qt::AlignHCenter);
    return combo;
}

void SceneOverlayToolbar::AddSeparator()
{
    auto* line = new QFrame(this);
    // A separator runs ACROSS the flow direction: a horizontal bar needs
    // vertical rules between its groups, and vice versa.
    line->setFrameShape(IsHorizontal() ? QFrame::VLine : QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet("color: rgb(78, 78, 78);");
    m_controlsLayout->addWidget(line);
}

void SceneOverlayToolbar::BindFloatProperty(std::function<float()> getter,
                                            std::function<void(float)> setter,
                                            const Properties::PropDesc& desc,
                                            const char* tooltip)
{
    auto property = std::unique_ptr<PropertyWidget<float>>(PropertyFactory::Create<float>(desc));
    QWidget* editor = property->GetWidget();
    editor->setFixedWidth(IsHorizontal() ? kFieldWidthHorizontal : kIconWidth);
    editor->setToolTip(QString::fromUtf8(tooltip));

    property->onChanged = [getter, setter](float value) {
        if (getter() != value) setter(value);
    };
    property->SetValue(getter());

    m_controlsLayout->addWidget(editor, 0, IsHorizontal() ? Qt::AlignVCenter : Qt::AlignHCenter);
    m_floatBindings.push_back({ std::move(property), std::move(getter), editor });
}

void SceneOverlayToolbar::Refresh()
{
    for (auto& binding : m_floatBindings) {
        const float value = binding.getter();
        // The focus guard is NOT a leftover from the old per-frame poll: any
        // external write (a script, MCP, the match-snap button) notifies while
        // the user may be part-way through typing a number, and writing the
        // stored value back would eat the keystrokes.
        if (!binding.widget->hasFocus() && binding.property->GetValue() != value)
            binding.property->SetValue(value);
    }
}

void SceneOverlayToolbar::SetCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    m_collapsed = collapsed;
    m_contents->setVisible(!collapsed);
    if (IsHorizontal())
        m_collapse->setArrowType(collapsed ? Qt::RightArrow : Qt::LeftArrow);
    else
        m_collapse->setArrowType(collapsed ? Qt::DownArrow : Qt::UpArrow);
    m_collapse->setToolTip(collapsed ? QStringLiteral("Expand toolbar")
                                     : QStringLiteral("Collapse toolbar"));
    layout()->activate();
    adjustSize();
    ClampToParent();
    SavePlacement();
}

void SceneOverlayToolbar::ClampToParent()
{
    QWidget* boundsWidget = parentWidget();
    if (!boundsWidget) return;

    QPoint target = pos();
    target.setX(std::clamp(target.x(), 0, std::max(0, boundsWidget->width() - width())));
    target.setY(std::clamp(target.y(), 0, std::max(0, boundsWidget->height() - height())));
    move(target);
}

void SceneOverlayToolbar::mousePressEvent(QMouseEvent* event)
{
    if (!m_movable || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_dragOffset = event->position().toPoint();
    setCursor(Qt::ClosedHandCursor);
    raise();
    event->accept();
}

void SceneOverlayToolbar::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging || !(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QWidget* boundsWidget = parentWidget();
    if (!boundsWidget) return;

    const QPoint cursorInParent = boundsWidget->mapFromGlobal(event->globalPosition().toPoint());
    move(cursorInParent - m_dragOffset);
    ClampToParent();
    event->accept();
}

void SceneOverlayToolbar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        SavePlacement();
        // A drag can finish with the pointer outside the bar, and the leaveEvent that
        // would normally fade it is suppressed while dragging. Settle the state here.
        if (m_opacity)
            m_opacity->setOpacity(underMouse() ? kFocusedOpacity : kUnfocusedOpacity);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void SceneOverlayToolbar::enterEvent(QEnterEvent* event)
{
    if (m_opacity) m_opacity->setOpacity(kFocusedOpacity);
    QWidget::enterEvent(event);
}

void SceneOverlayToolbar::leaveEvent(QEvent* event)
{
    // Two cases where a leave must NOT fade the bar:
    //
    //  - Mid-drag. The pointer routinely travels outside a widget it is dragging, and
    //    the bar going ghost under the cursor while you move it is disorienting.
    //  - The pointer moved onto one of our own children. Qt delivers leaveEvent to this
    //    widget when the cursor crosses into a child button, so without the geometry
    //    check the bar would flicker every time you reached for a control on it.
    if (m_dragging)
        return;
    if (QWidget* host = parentWidget()) {
        if (geometry().contains(host->mapFromGlobal(QCursor::pos())))
            return;
    }

    if (m_opacity) m_opacity->setOpacity(kUnfocusedOpacity);
    QWidget::leaveEvent(event);
}

void SceneOverlayToolbar::SavePlacement() const
{
    if (m_settingsKey.isEmpty()) return;

    // Same organisation/application as MainWindow's layout, so a user clearing editor
    // layout state clears all of it from one place.
    QSettings settings(QStringLiteral("Rocklyn"), QStringLiteral("RockEngineEditor"));
    settings.setValue(QStringLiteral("sceneToolbars/%1/pos").arg(m_settingsKey), pos());
    settings.setValue(QStringLiteral("sceneToolbars/%1/collapsed").arg(m_settingsKey), m_collapsed);
}

void SceneOverlayToolbar::RestorePlacement()
{
    if (m_settingsKey.isEmpty()) return;

    QSettings settings(QStringLiteral("Rocklyn"), QStringLiteral("RockEngineEditor"));

    const QVariant collapsed = settings.value(QStringLiteral("sceneToolbars/%1/collapsed").arg(m_settingsKey));
    if (collapsed.isValid())
        SetCollapsed(collapsed.toBool());   // resizes the bar, so it must precede the move

    const QVariant stored = settings.value(QStringLiteral("sceneToolbars/%1/pos").arg(m_settingsKey));
    if (stored.isValid()) {
        move(stored.toPoint());
        // The viewport may currently be smaller than it was when this was saved (or not
        // laid out yet), which would strand the bar off-screen. The host also re-clamps
        // on every resize, so a zero-size parent here is corrected shortly after.
        ClampToParent();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// SceneToolsToolbar
// ═════════════════════════════════════════════════════════════════════════════

SceneToolsToolbar::SceneToolsToolbar(QWidget* parent)
    : SceneOverlayToolbar(parent, Qt::Vertical, QStringLiteral("tools"))
{
    m_hand = AddButton(ICON_FA_HAND, "Hand tool");
    connect(m_hand, &QToolButton::clicked, this, [] {
        SetTransformOperation(ImGuizmo::OPERATION(-1));
    });

    AddSeparator();

    m_universal = AddButton(ICON_FA_ARROWS_TO_DOT, "Universal transform");
    connect(m_universal, &QToolButton::clicked, this, [] {
        SetTransformOperation(ImGuizmo::UNIVERSAL);
    });

    m_translate = AddButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Move");
    connect(m_translate, &QToolButton::clicked, this, [] {
        SetTransformOperation(ImGuizmo::TRANSLATE);
    });

    m_rotate = AddButton(ICON_FA_ROTATE, "Rotate");
    connect(m_rotate, &QToolButton::clicked, this, [] {
        SetTransformOperation(ImGuizmo::ROTATE);
    });

    m_scale = AddButton(ICON_FA_MAXIMIZE, "Scale");
    connect(m_scale, &QToolButton::clicked, this, [] {
        SetTransformOperation(ImGuizmo::SCALE);
    });

    m_spriteBox = AddButton(ICON_FA_CROP_SIMPLE,
                            "Box edit sprite\nDrag handles to scale, drag inside to move");
    connect(m_spriteBox, &QToolButton::clicked, this, [] {
        auto* gizmos = GizmosManager::Get();
        const bool active = gizmos->GetEditMode() == GizmosManager::EditMode::SpriteBox;
        gizmos->SetEditMode(active ? GizmosManager::EditMode::Transform
                                   : GizmosManager::EditMode::SpriteBox);
    });

    m_collider = AddButton(ICON_FA_DRAW_POLYGON, "Edit collider");
    connect(m_collider, &QToolButton::clicked, this, [] {
        auto* gizmos = GizmosManager::Get();
        const bool active = gizmos->GetEditMode() == GizmosManager::EditMode::Collider;
        gizmos->SetEditMode(active ? GizmosManager::EditMode::Transform
                                   : GizmosManager::EditMode::Collider);
    });

    // The active tool. GizmosManager is process-global (it extends System but is
    // never registered in a Container), so this subscription outlives play mode.
    SubscribeTo(GizmosManager::Get(), GizmosManager::TOOL_CHANGED_EVENT,
                [this] { Refresh(); });

    // The selection, which is NOT process-global. Same shape as
    // InspectorGui::Init, and the asymmetry between enter and exit is deliberate
    // -- see the comments there and on the two handlers below.
    auto* engine = Engine::Get();

    // Entering play mode: the runtime container's SelectionManager is a fresh
    // copy, and Copy(Container*) does not carry subscribers, so subscribe to it.
    SubscribeTo(engine, Engine::ENTER_PLAY_MODE_EVENT, [this] { SubscribeToSelector(); });

    // Exiting play mode: do NOT re-subscribe. The editor SelectionManager still
    // holds the subscription made in the constructor (the editor container
    // outlives play mode), and re-subscribing would stack a duplicate handler
    // every play cycle.
    SubscribeTo(engine, Engine::EXIT_PLAY_MODE_EVENT, [this] {
        // ExitPlayMode() has already deleted the runtime container, so any
        // per-object subscriptions taken during play point at freed objects.
        // Their ~Observable already dropped them; the handles merely dangle.
        // Re-point WITHOUT unsubscribing the old ones -- calling Unsubscribe on
        // a freed object is a use-after-free. This is the ONLY caller that
        // passes false; every other path unsubscribes normally.
        RewireSelectionSubs(/*unsubscribeOld=*/false);
        Refresh();
    });

    SubscribeToSelector();
    Refresh();
}

SceneToolsToolbar::~SceneToolsToolbar()
{
    // Reached on editor shutdown, where the editor container's objects are still
    // alive, so a normal unsubscribe is correct here.
    for (auto& [source, id] : m_objectSubs)
        if (source) source->Unsubscribe(id);
    m_objectSubs.clear();
}

void SceneToolsToolbar::SubscribeToSelector()
{
    if (!m_selection) return;

    // Deliberately NOT routed through SubscribeTo, and this is the one place in
    // this file where that matters.
    //
    // SelectionManager is Container-scoped, so the manager subscribed to on the
    // enter-play path is destroyed again on exit. Recording it for the
    // destructor to unsubscribe would mean calling Unsubscribe on freed memory
    // once per play cycle at shutdown. Nothing leaks by skipping it: a destroyed
    // Observable drops its own subscriber list in ~Observable.
    //
    // Same reasoning, and the same untracked call, as InspectorGui::SubscribeToSelector.
    m_selection->Subscribe([this]() {
        OnSelectionChanged();
        return true;
    }, SelectionManager::SELECTION_CHANGED_EVENT);

    OnSelectionChanged();
}

void SceneToolsToolbar::OnSelectionChanged()
{
    RewireSelectionSubs(/*unsubscribeOld=*/true);
    Refresh();
}

void SceneToolsToolbar::RewireSelectionSubs(bool unsubscribeOld)
{
    if (unsubscribeOld) {
        for (auto& [source, id] : m_objectSubs)
            if (source) source->Unsubscribe(id);
    }
    m_objectSubs.clear();

    if (!m_selection) return;
    auto* object = dynamic_cast<GameObject*>(m_selection->GetSerializable());
    if (!object) return;

    // Adding a collider to the ALREADY-selected object has to make the collider
    // button appear; the selection itself never changed, so SELECTION_CHANGED
    // does not cover it. The old per-frame poll got this for free.
    for (const Observable::Event event : { GameObject::ADD_COMPONENT_EVENT,
                                           GameObject::REMOVE_COMPONENT_EVENT }) {
        const int id = object->Subscribe([this]() {
            Refresh();
            return true;
        }, event);
        m_objectSubs.emplace_back(object, id);
    }
}

void SceneToolsToolbar::Refresh()
{
    SceneOverlayToolbar::Refresh();

    auto* gizmos = GizmosManager::Get();
    const bool transformMode = gizmos->GetEditMode() == GizmosManager::EditMode::Transform;
    const ImGuizmo::OPERATION operation = gizmos->GetOperation();

    // Radio-style exclusivity by hand rather than a QActionGroup: the engine's
    // operation is the single source of truth and exactly one button matches it.
    SetChecked(m_hand,      transformMode && operation == ImGuizmo::OPERATION(-1));
    SetChecked(m_universal, transformMode && operation == ImGuizmo::UNIVERSAL);
    SetChecked(m_translate, transformMode && operation == ImGuizmo::TRANSLATE);
    SetChecked(m_rotate,    transformMode && operation == ImGuizmo::ROTATE);
    SetChecked(m_scale,     transformMode && operation == ImGuizmo::SCALE);

    auto* object = m_selection ? dynamic_cast<GameObject*>(m_selection->GetSerializable())
                               : nullptr;
    const bool hasSprite = object && object->GetComponent<SpriteRenderer>();
    const bool hasCollider = object && (object->GetComponent<BoxCollider>() ||
                                        object->GetComponent<CircleCollider>() ||
                                        object->GetComponent<CapsuleCollider>());

    // isVisibleTo(this), not isHidden(): Refresh runs once from the constructor,
    // before this widget is ever shown, and isHidden() is true for every
    // not-yet-shown child regardless of what it will do when the parent appears.
    // isVisibleTo answers the question actually being asked -- "would this show
    // if the parent were shown" -- which is well defined before the first show.
    // setVisible itself stays unconditional (it is idempotent); only the
    // relayout is gated.
    const bool sizeChanged = m_spriteBox->isVisibleTo(this) != hasSprite ||
                             m_collider->isVisibleTo(this)  != hasCollider;
    m_spriteBox->setVisible(hasSprite);
    m_collider->setVisible(hasCollider);
    SetChecked(m_spriteBox, gizmos->GetEditMode() == GizmosManager::EditMode::SpriteBox);
    SetChecked(m_collider,  gizmos->GetEditMode() == GizmosManager::EditMode::Collider);

    if (sizeChanged) {
        adjustSize();
        ClampToParent();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// SceneViewOptionsToolbar
// ═════════════════════════════════════════════════════════════════════════════

SceneViewOptionsToolbar::SceneViewOptionsToolbar(QWidget* parent)
    : SceneOverlayToolbar(parent, Qt::Horizontal, QStringLiteral("viewOptions"))
{
    m_lighting = AddButton(ICON_FA_LIGHTBULB,
                           "Lighting\nOff renders the Scene view unshaded. The Game view "
                           "and play mode stay lit.");
    connect(m_lighting, &QToolButton::clicked, this, [](bool checked) {
        EditSetting<bool>("sceneview.lighting", "Toggle Scene Lighting",
            [] { return SceneViewGui::Get()->IsSceneLightingEnabled(); },
            [](const bool& v) { SceneViewGui::Get()->SetSceneLightingEnabled(v); },
            checked);
    });

    AddSeparator();

    m_gizmos = AddButton(ICON_FA_EYE,
                         "Gizmos\nShow camera, light, audio and icon overlays. Transform "
                         "gizmos are always shown.");
    connect(m_gizmos, &QToolButton::clicked, this, [](bool checked) {
        EditSetting<bool>("gizmos.enabled", "Toggle Gizmos",
            [] { return GizmoSettings::Get().IsEnabled(); },
            [](const bool& v) { GizmoSettings::Get().SetEnabled(v); },
            checked);
    });

    m_gizmoMenu = AddButton(ICON_FA_CARET_DOWN, "Choose which gizmos are shown");
    m_gizmoMenu->setCheckable(false);
    m_gizmoMenu->setFixedWidth(16);
    connect(m_gizmoMenu, &QToolButton::clicked, this, [this] { ShowGizmoMenu(); });

    AddSeparator();

    m_snap = AddButton(ICON_FA_MAGNET,
                       "Snap\nAlways snap using the increments below; Ctrl snaps one drag.");
    connect(m_snap, &QToolButton::clicked, this, [](bool checked) {
        EditSetting<bool>("grid.snapEnabled", "Toggle Snapping",
            [] { return GridSettings::Get().IsSnapEnabled(); },
            [](const bool& v) { GridSettings::Get().SetSnapEnabled(v); },
            checked);
    });

    m_grid = AddButton(ICON_FA_BORDER_ALL,
                       "Grid visibility\nHiding the grid does not disable snapping.");
    connect(m_grid, &QToolButton::clicked, this, [](bool checked) {
        EditSetting<bool>("grid.visible", "Toggle Grid",
            [] { return GridSettings::Get().IsVisible(); },
            [](const bool& v) { GridSettings::Get().SetVisible(v); },
            checked);
    });

    AddSeparator();

    // The four numeric fields record as MERGEABLE edits: a spinbox fires on every
    // keystroke and every arrow repeat, so typing "4096" would otherwise leave
    // four separate entries ("4", "40", "409", "4096") for one edit.
    BindFloatProperty(
        [] { return GridSettings::Get().GetCellSize(); },
        [](float value) {
            EditSetting<float>("grid.cellSize", "Change Cell Size",
                [] { return GridSettings::Get().GetCellSize(); },
                [](const float& v) { GridSettings::Get().SetCellSize(v); },
                value, /*mergeable=*/true);
        },
        Properties::PropDesc().Tag(Properties::Tags::INT).Range(1.0f, 4096.0f).Step(1.0f),
        "Cell size\nGrid spacing in world units.");

    BindFloatProperty(
        [] { return GridSettings::Get().GetMoveSnap(); },
        [](float value) {
            EditSetting<float>("grid.moveSnap", "Change Move Snap",
                [] { return GridSettings::Get().GetMoveSnap(); },
                [](const float& v) { GridSettings::Get().SetMoveSnap(v); },
                value, /*mergeable=*/true);
        },
        Properties::PropDesc().Tag(Properties::Tags::FLOAT).Range(0.01f, 4096.0f).Step(0.01f),
        "Move snap\nIncrement in world units.");

    BindFloatProperty(
        [] { return GridSettings::Get().GetRotateSnap(); },
        [](float value) {
            EditSetting<float>("grid.rotateSnap", "Change Rotate Snap",
                [] { return GridSettings::Get().GetRotateSnap(); },
                [](const float& v) { GridSettings::Get().SetRotateSnap(v); },
                value, /*mergeable=*/true);
        },
        Properties::PropDesc().Tag(Properties::Tags::INT).Range(1.0f, 180.0f).Step(1.0f),
        "Rotate snap\nIncrement in degrees.");

    BindFloatProperty(
        [] { return GridSettings::Get().GetScaleSnap(); },
        [](float value) {
            EditSetting<float>("grid.scaleSnap", "Change Scale Snap",
                [] { return GridSettings::Get().GetScaleSnap(); },
                [](const float& v) { GridSettings::Get().SetScaleSnap(v); },
                value, /*mergeable=*/true);
        },
        Properties::PropDesc().Tag(Properties::Tags::FLOAT).Range(0.01f, 10.0f).Step(0.01f),
        "Scale snap\nIncrement as a multiplier.");

    m_match = AddButton(ICON_FA_LINK,
                        "Match move snap to cell size\nObjects will land on grid lines.");
    m_match->setCheckable(false);
    connect(m_match, &QToolButton::clicked, this, [] {
        // Recorded as a move-snap edit rather than as "run MatchMoveSnapToCell":
        // undoing has to restore the PREVIOUS move snap, and re-running the match
        // in reverse cannot express that. Not mergeable -- one click, one entry.
        EditSetting<float>("grid.moveSnap", "Match Move Snap To Cell",
            [] { return GridSettings::Get().GetMoveSnap(); },
            [](const float& v) { GridSettings::Get().SetMoveSnap(v); },
            GridSettings::Get().GetCellSize());
    });

    // No selection dependency here, so both sources are process-global singletons
    // that outlive the play-mode swap -- no rewiring, no lifetime hazards.
    SubscribeTo(&GridSettings::Get(),  GridSettings::CHANGED_EVENT,  [this] { Refresh(); });
    SubscribeTo(&GizmoSettings::Get(), GizmoSettings::CHANGED_EVENT, [this] { Refresh(); });

    Refresh();
}

void SceneViewOptionsToolbar::ShowGizmoMenu()
{
    auto& settings = GizmoSettings::Get();

    QMenu menu(this);
    menu.setStyleSheet(QString::fromUtf8(kMenuStyle));

    // Built by looping to Count rather than listed here, so adding a category in
    // GizmoSettings shows up without an edit on this side.
    for (int i = 0; i < static_cast<int>(GizmoSettings::Category::Count); ++i) {
        const auto category = static_cast<GizmoSettings::Category>(i);
        QAction* action = menu.addAction(
            QString::fromUtf8(GizmoSettings::CategoryLabel(category)));
        action->setCheckable(true);
        action->setChecked(settings.IsCategoryVisible(category));
        // Greyed rather than hidden when the master toggle is off: the ticks
        // still show which set will come back when it is switched on again.
        action->setEnabled(settings.IsEnabled());
        connect(action, &QAction::toggled, this, [category](bool checked) {
            // The key is per-category so two categories toggled in quick
            // succession can never fold into each other -- though nothing here
            // is mergeable anyway, since each tick is one deliberate action.
            const std::string label = GizmoSettings::CategoryLabel(category);
            EditSetting<bool>("gizmos.category." + label,
                "Toggle " + label + " Gizmos",
                [category] { return GizmoSettings::Get().IsCategoryVisible(category); },
                [category](const bool& v) {
                    GizmoSettings::Get().SetCategoryVisible(category, v);
                },
                checked);
        });
    }

    // This DOES spin a nested QEventLoop, so it is worth being explicit about
    // why the "never exec() a dialog" rule in Editor/CLAUDE.md does not bite.
    //
    // That rule exists because a nested loop keeps firing frameSwapped ->
    // FrameTick -> Engine::Update -> JobSystem::Pump; the hazard is re-entering
    // the pump from INSIDE a job step, which is how LoadingOverlay and
    // QProgressDialog got themselves banned. This menu opens from an ordinary
    // user click on the Qt event loop, never from within a job, so the pump runs
    // normally rather than re-entrantly -- the same position the existing
    // button-menu idiom in ProperyFactory.hpp occupies.
    //
    // If this ever needs to be opened from inside a job step, switch to
    // QMenu::popup() (non-blocking, needs an owned menu) rather than relaxing
    // the rule.
    menu.exec(mapToGlobal(m_gizmoMenu->geometry().bottomLeft()));
}

void SceneViewOptionsToolbar::Refresh()
{
    SceneOverlayToolbar::Refresh();

    SetChecked(m_lighting, SceneViewGui::Get()->IsSceneLightingEnabled());

    auto& gizmoSettings = GizmoSettings::Get();
    SetChecked(m_gizmos, gizmoSettings.IsEnabled());

    auto& grid = GridSettings::Get();
    SetChecked(m_snap, grid.IsSnapEnabled());
    SetChecked(m_grid, grid.IsVisible());

    // The link button is only meaningful while the two values disagree.
    // isVisibleTo(this) rather than isHidden() -- see the note in
    // SceneToolsToolbar::Refresh. setVisible is unconditional; only the relayout
    // is gated on an actual change.
    const bool showMatch = grid.GetMoveSnap() != grid.GetCellSize();
    const bool sizeChanged = m_match->isVisibleTo(this) != showMatch;
    m_match->setVisible(showMatch);
    if (sizeChanged) {
        adjustSize();
        ClampToParent();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// GameViewToolbar
// ═════════════════════════════════════════════════════════════════════════════

GameViewToolbar::GameViewToolbar(QWidget* parent)
    // Empty settings key: a pinned bar has no placement worth remembering.
    : SceneOverlayToolbar(parent, Qt::Horizontal, QString(), /*movable=*/false)
{
    m_combo = AddComboBox("Aspect ratio / resolution", 190);

    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int row) {
        if (!onIndexChanged || row < 0) return;
        const QVariant payload = m_combo->itemData(row);
        if (!payload.isValid()) return;      // a separator row
        onIndexChanged(payload.toInt());
    });
}

void GameViewToolbar::SetItems(const QStringList& labels, int separatorBefore)
{
    QSignalBlocker blocker(m_combo);   // populating must not fire onIndexChanged
    m_combo->clear();

    for (int i = 0; i < labels.size(); ++i) {
        if (i == separatorBefore)
            m_combo->insertSeparator(m_combo->count());
        // The caller's index travels as item data. A separator occupies a row of its
        // own, so past that point row != index and reading currentIndex() would select
        // the wrong preset -- this is what keeps the two from drifting.
        m_combo->addItem(labels.at(i), i);
    }
}

void GameViewToolbar::SetCurrentIndex(int index)
{
    QSignalBlocker blocker(m_combo);
    const int row = m_combo->findData(index);
    if (row >= 0) m_combo->setCurrentIndex(row);
}
