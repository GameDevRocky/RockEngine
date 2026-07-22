#include "dock-widgets/AnimatorGraphCanvas.hpp"
#include "engine/components/Animator.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QtMath>
#include <cmath>

namespace {
    // Distance from point p to segment ab.
    double DistToSegment(const QPointF& p, const QPointF& a, const QPointF& b) {
        const QPointF ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        if (len2 < 1e-6) return std::hypot(p.x() - a.x(), p.y() - a.y());
        double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF proj(a.x() + t * ab.x(), a.y() + t * ab.y());
        return std::hypot(p.x() - proj.x(), p.y() - proj.y());
    }

    // A small perpendicular offset so A->B and B->A don't overlap.
    QPointF PerpOffset(const QPointF& a, const QPointF& b, double amount) {
        QPointF d = b - a;
        double len = std::hypot(d.x(), d.y());
        if (len < 1e-3) return QPointF(0, 0);
        return QPointF(-d.y() / len * amount, d.x() / len * amount);
    }
}

AnimatorGraphCanvas::AnimatorGraphCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 200);
}

void AnimatorGraphCanvas::SetAnimator(Animator* a) {
    if (m_animator == a) return;
    m_animator = a;
    ClearSelection();
    CancelMakeTransition();
    update();
}

void AnimatorGraphCanvas::SelectState(const std::string& name) {
    m_selectedState = name;
    m_selectedTransitionId.clear();
    update();
}
void AnimatorGraphCanvas::SelectTransition(const std::string& id) {
    m_selectedTransitionId = id;
    m_selectedState.clear();
    update();
}
void AnimatorGraphCanvas::ClearSelection() {
    m_selectedState.clear();
    m_selectedTransitionId.clear();
    update();
}

// ─── coordinate conversions ───────────────────────────────────────────────────

QPointF AnimatorGraphCanvas::GraphToScreen(const QPointF& g) const {
    return QPointF(g.x() * m_zoom + m_panOffset.x(), g.y() * m_zoom + m_panOffset.y());
}
QPointF AnimatorGraphCanvas::ScreenToGraph(const QPointF& s) const {
    return QPointF((s.x() - m_panOffset.x()) / m_zoom, (s.y() - m_panOffset.y()) / m_zoom);
}
QRectF AnimatorGraphCanvas::NodeScreenRect(const QPointF& centerGraph) const {
    const QPointF c = GraphToScreen(centerGraph);
    const float w = kNodeW * m_zoom, h = kNodeH * m_zoom;
    return QRectF(c.x() - w / 2, c.y() - h / 2, w, h);
}

QPointF AnimatorGraphCanvas::StateCenter(const std::string& name) const {
    if (m_animator)
        for (const auto& s : m_animator->GetStates())
            if (s.name == name) return QPointF(s.editorPos.x, s.editorPos.y);
    return QPointF(0, 0);
}

// ─── hit-testing ──────────────────────────────────────────────────────────────

std::string AnimatorGraphCanvas::StateAtScreen(const QPointF& s) const {
    if (!m_animator) return {};
    const auto& states = m_animator->GetStates();
    // topmost (drawn last) wins
    for (auto it = states.rbegin(); it != states.rend(); ++it)
        if (NodeScreenRect(QPointF(it->editorPos.x, it->editorPos.y)).contains(s)) return it->name;
    return {};
}
bool AnimatorGraphCanvas::AnyStateAtScreen(const QPointF& s) const {
    return NodeScreenRect(m_anyStatePos).contains(s);
}
std::string AnimatorGraphCanvas::TransitionAtScreen(const QPointF& s) const {
    if (!m_animator) return {};
    for (const auto& t : m_animator->GetTransitions()) {
        const QPointF srcC = t.fromAnyState ? m_anyStatePos : StateCenter(t.fromState);
        const QPointF dstC = StateCenter(t.toState);
        QPointF a = GraphToScreen(srcC), b = GraphToScreen(dstC);
        const QPointF off = PerpOffset(a, b, 5.0);
        a += off; b += off;
        if (DistToSegment(s, a, b) <= 6.0) return t.id;
    }
    return {};
}

// ─── painting ─────────────────────────────────────────────────────────────────

void AnimatorGraphCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(37, 37, 38));

    if (!m_animator) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, "No Animator");
        return;
    }

    // Subtle grid.
    p.setPen(QColor(48, 48, 50));
    const float step = 32.0f * m_zoom;
    if (step > 6.0f) {
        for (float x = std::fmod(m_panOffset.x(), step); x < width(); x += step)
            p.drawLine(QPointF(x, 0), QPointF(x, height()));
        for (float y = std::fmod(m_panOffset.y(), step); y < height(); y += step)
            p.drawLine(QPointF(0, y), QPointF(width(), y));
    }

    const std::string& current = m_animator->GetCurrentState();

    // Transitions first (under the nodes).
    for (const auto& t : m_animator->GetTransitions()) {
        const QPointF srcC = t.fromAnyState ? m_anyStatePos : StateCenter(t.fromState);
        const QPointF dstC = StateCenter(t.toState);
        QPointF a = GraphToScreen(srcC), b = GraphToScreen(dstC);
        const QPointF off = PerpOffset(a, b, 5.0);
        a += off; b += off;

        const bool sel = (t.id == m_selectedTransitionId);
        p.setPen(QPen(sel ? QColor(80, 160, 255) : QColor(190, 190, 190), sel ? 2.5 : 1.6));
        p.drawLine(a, b);

        // Arrowhead at the midpoint, pointing toward the target.
        const QPointF mid = (a + b) / 2.0;
        const double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
        const double ah = 9.0;
        QPointF p1(mid.x() - ah * std::cos(ang - 0.5), mid.y() - ah * std::sin(ang - 0.5));
        QPointF p2(mid.x() - ah * std::cos(ang + 0.5), mid.y() - ah * std::sin(ang + 0.5));
        QPainterPath head; head.moveTo(mid); head.lineTo(p1); head.lineTo(p2); head.closeSubpath();
        p.fillPath(head, sel ? QColor(80, 160, 255) : QColor(210, 210, 210));
    }

    // Pending "make transition" rubber band.
    if (m_makingTransition) {
        const QPointF srcC = m_makingFromAny ? m_anyStatePos : StateCenter(m_transitionSource);
        p.setPen(QPen(QColor(80, 160, 255), 1.6, Qt::DashLine));
        p.drawLine(GraphToScreen(srcC), GraphToScreen(m_cursorGraph));
    }

    // Any State node.
    {
        const QRectF r = NodeScreenRect(m_anyStatePos);
        p.setBrush(QColor(60, 92, 60));
        p.setPen(QPen(QColor(120, 170, 120), 1.5));
        p.drawRoundedRect(r, 6, 6);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, "Any State");
    }

    // State nodes.
    const std::string& def = m_animator->GetDefaultState();
    for (const auto& s : m_animator->GetStates()) {
        const QRectF r = NodeScreenRect(QPointF(s.editorPos.x, s.editorPos.y));
        const bool isDefault = (s.name == def);
        const bool isCurrent = (!current.empty() && s.name == current);
        const bool isSelected = (s.name == m_selectedState);

        QColor fill = isCurrent ? QColor(70, 130, 80)
                    : isDefault ? QColor(150, 105, 45)
                                : QColor(66, 66, 72);
        p.setBrush(fill);
        p.setPen(QPen(isSelected ? QColor(90, 170, 255) : QColor(30, 30, 32), isSelected ? 2.5 : 1.2));
        p.drawRoundedRect(r, 6, 6);

        p.setPen(Qt::white);
        QFont f = p.font(); f.setBold(true); p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QString::fromStdString(s.name));
    }
}

// ─── interaction ──────────────────────────────────────────────────────────────

void AnimatorGraphCanvas::mousePressEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    m_lastMouse = pos;

    if (e->button() == Qt::MiddleButton) { m_drag = Drag::Pan; return; }

    if (e->button() == Qt::LeftButton) {
        // Completing a "make transition" gesture: click a target state.
        if (m_makingTransition) {
            const std::string target = StateAtScreen(pos);
            if (!target.empty() && m_animator &&
                (m_makingFromAny || target != m_transitionSource)) {
                m_animator->AddTransition(m_transitionSource, target, m_makingFromAny);
                emit graphChanged();
            }
            CancelMakeTransition();
            update();
            return;
        }

        // Select / drag a node (states first, then the Any State node).
        const std::string st = StateAtScreen(pos);
        if (!st.empty()) {
            SelectState(st);
            emit stateSelected(QString::fromStdString(st));
            if (m_editable) {
                m_drag = Drag::MoveNode;
                m_dragState = st;
                m_grabOffset = ScreenToGraph(pos) - StateCenter(st);
            }
            return;
        }
        if (AnyStateAtScreen(pos)) {
            if (m_editable) { m_drag = Drag::MoveAnyState; m_grabOffset = ScreenToGraph(pos) - m_anyStatePos; }
            return;
        }

        // Select a transition?
        const std::string tr = TransitionAtScreen(pos);
        if (!tr.empty()) {
            SelectTransition(tr);
            emit transitionSelected(QString::fromStdString(tr));
            return;
        }

        // Empty space: clear selection.
        ClearSelection();
        emit selectionCleared();
        return;
    }
}

void AnimatorGraphCanvas::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();

    if (m_makingTransition) { m_cursorGraph = ScreenToGraph(pos); update(); }

    switch (m_drag) {
        case Drag::Pan: {
            m_panOffset += pos - m_lastMouse;
            m_lastMouse = pos;
            update();
            break;
        }
        case Drag::MoveNode: {
            if (m_animator)
                if (AnimatorState* s = m_animator->FindState(m_dragState)) {
                    const QPointF c = ScreenToGraph(pos) - m_grabOffset;
                    s->editorPos = glm::vec2(static_cast<float>(c.x()), static_cast<float>(c.y()));
                    update();
                }
            break;
        }
        case Drag::MoveAnyState: {
            m_anyStatePos = ScreenToGraph(pos) - m_grabOffset;
            update();
            break;
        }
        case Drag::None:
            break;
    }
}

void AnimatorGraphCanvas::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) {
        ShowContextMenu(e->globalPosition().toPoint(), e->position().toPoint());
        return;
    }
    if (m_drag == Drag::MoveNode) emit graphChanged();   // persisted node position changed
    m_drag = Drag::None;
    m_dragState.clear();
}

void AnimatorGraphCanvas::wheelEvent(QWheelEvent* e) {
    const QPointF cursor = e->position();
    const QPointF before = ScreenToGraph(cursor);
    const double factor = (e->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    m_zoom = std::clamp(static_cast<float>(m_zoom * factor), 0.25f, 3.0f);
    // keep the point under the cursor fixed
    const QPointF after = GraphToScreen(before);
    m_panOffset += cursor - after;
    update();
}

// ─── context menu ─────────────────────────────────────────────────────────────

void AnimatorGraphCanvas::ShowContextMenu(const QPoint& screenPos, const QPoint& localPos) {
    if (!m_animator) return;
    const QPointF local(localPos);
    QMenu menu(this);

    const std::string overState = StateAtScreen(local);
    const bool overAny = AnyStateAtScreen(local);
    const std::string overTransition = overState.empty() ? TransitionAtScreen(local) : std::string();

    if (!overState.empty()) {
        if (m_editable) {
            menu.addAction("Make Transition", this, [this, overState]() { BeginMakeTransition(overState, false); });
            menu.addAction("Set as Default State", this, [this, overState]() {
                m_animator->SetDefaultState(overState);
                emit graphChanged();
                update();
            });
            menu.addSeparator();
            menu.addAction("Delete State", this, [this, overState]() {
                m_animator->RemoveState(overState);
                ClearSelection();
                emit selectionCleared();
                emit graphChanged();
                update();
            });
        }
    } else if (overAny) {
        if (m_editable)
            menu.addAction("Make Transition", this, [this]() { BeginMakeTransition(std::string(), true); });
    } else if (!overTransition.empty()) {
        if (m_editable)
            menu.addAction("Delete Transition", this, [this, overTransition]() {
                m_animator->RemoveTransition(overTransition);
                ClearSelection();
                emit selectionCleared();
                emit graphChanged();
                update();
            });
    } else {
        if (m_editable) {
            const QPointF g = ScreenToGraph(local);
            menu.addAction("Add State", this, [this, g]() {
                AnimatorState* s = m_animator->AddState("New State",
                    glm::vec2(static_cast<float>(g.x()), static_cast<float>(g.y())));
                if (s) { SelectState(s->name); emit stateSelected(QString::fromStdString(s->name)); }
                emit graphChanged();
                update();
            });
        }
    }

    if (!menu.isEmpty()) menu.exec(screenPos);
}

void AnimatorGraphCanvas::BeginMakeTransition(const std::string& fromState, bool fromAny) {
    m_makingTransition = true;
    m_makingFromAny = fromAny;
    m_transitionSource = fromState;
    m_cursorGraph = fromAny ? m_anyStatePos : StateCenter(fromState);
    setCursor(Qt::CrossCursor);
    update();
}

void AnimatorGraphCanvas::CancelMakeTransition() {
    if (!m_makingTransition) return;
    m_makingTransition = false;
    m_makingFromAny = false;
    m_transitionSource.clear();
    unsetCursor();
}
