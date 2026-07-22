#pragma once
#include <QWidget>
#include <QPointF>
#include <QString>
#include <string>

class Animator;

// Custom-painted node-graph canvas for the Animator state machine (modeled on
// SpriteCanvasWidget: zoom + pan, hit-testing, a Drag state machine, and
// "mutate engine object -> emit changed"). States are draggable nodes,
// transitions are arrows. Structural edits go through the Animator's mutators.
// Read-only in play mode; highlights the live current state instead.
class AnimatorGraphCanvas : public QWidget {
    Q_OBJECT
public:
    explicit AnimatorGraphCanvas(QWidget* parent = nullptr);

    void SetAnimator(Animator* a);           // the animator being edited (nullptr clears)
    void SetEditable(bool on) { m_editable = on; if (!on) CancelMakeTransition(); update(); }

    void SelectState(const std::string& name);
    void SelectTransition(const std::string& id);
    void ClearSelection();

signals:
    void stateSelected(const QString& name);
    void transitionSelected(const QString& id);
    void selectionCleared();
    void graphChanged();                     // a structural edit happened on the canvas

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    enum class Drag { None, MoveNode, MoveAnyState, Pan };

    // coordinate conversions (graph space <-> screen)
    QPointF GraphToScreen(const QPointF& g) const;
    QPointF ScreenToGraph(const QPointF& s) const;
    QRectF  NodeScreenRect(const QPointF& centerGraph) const;

    QPointF StateCenter(const std::string& name) const;   // graph-space center of a state node
    std::string StateAtScreen(const QPointF& s) const;    // "" if none
    bool        AnyStateAtScreen(const QPointF& s) const;
    std::string TransitionAtScreen(const QPointF& s) const;

    void ShowContextMenu(const QPoint& screenPos, const QPoint& localPos);
    void BeginMakeTransition(const std::string& fromState, bool fromAny);
    void CancelMakeTransition();

    Animator* m_animator = nullptr;
    bool m_editable = true;

    float   m_zoom = 1.0f;
    QPointF m_panOffset{80, 80};
    QPointF m_anyStatePos{-40, -70};   // graph pos of the Any State node (session-local, not saved)

    std::string m_selectedState;
    std::string m_selectedTransitionId;

    Drag    m_drag = Drag::None;
    QPointF m_lastMouse{0, 0};          // for pan
    QPointF m_grabOffset{0, 0};         // node-center -> cursor (graph space) while moving
    std::string m_dragState;

    // "Make Transition" mode: rubber-band from a source node to the next clicked node.
    bool m_makingTransition = false;
    bool m_makingFromAny = false;
    std::string m_transitionSource;
    QPointF m_cursorGraph{0, 0};

    static constexpr float kNodeW = 150.0f;
    static constexpr float kNodeH = 44.0f;
};
