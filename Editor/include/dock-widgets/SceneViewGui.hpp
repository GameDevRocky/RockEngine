#pragma once
#include "dock-widgets/ViewportWidget.hpp"   // must precede any Qt header that pulls GL headers (glad.h ordering)
#include <QWheelEvent>
#include <QPoint>
#include <QRect>
#include <QMouseEvent>
#include <QRubberBand>
#include <glm/glm.hpp>
#include "engine/rendering/views/EditorRenderView.hpp"
#include "utils/ImGuiInstance.hpp"

class SceneViewGui : public ViewportWidget {
    Q_OBJECT

public:
    static SceneViewGui* Get() {
        static SceneViewGui* instance = nullptr;
        if (!instance) {
            instance = new SceneViewGui(nullptr);
        }
        return instance;
    }

    void Init();

    // Centre the editor camera on the current selection and adjust zoom so it fills
    // the view. Public so other panels (e.g. the Hierarchy) can trigger "frame
    // selected" via SceneViewGui::Get()->FrameSelection().
    void FrameSelection();

protected:
    void DrawGizmos();
    void DrawToolBar();

    RenderView* CreateView(int pixelW, int pixelH) override;
    void OnViewInitialized() override;
    void OnResized(int logicalW, int logicalH) override;
    void OnAfterPresent() override;

    // Accept .scene files dragged from the Folder view and load them, mirroring
    // HierarchyGui's drop handling so a scene can be loaded onto either panel.
    // Also: sprites (spawn an object) and materials (live-preview + assign on drop).
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    // Alt+Left or middle-mouse. Ctrl is reserved for additive selection so it means
    // the same thing here as in the hierarchy tree.
    bool IsPanGesture(Qt::MouseButton button, Qt::KeyboardModifiers modifiers) const;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit SceneViewGui(QWidget* parent = nullptr);

    // Spawn a GameObject with a SpriteRenderer (set to spriteId) in the first loaded
    // scene at the given view position. Returns false if no scene is loaded.
    bool SpawnSpriteObject(const std::string& spriteId, const QPointF& viewPos);

    // ── Material drag preview ────────────────────────────────────────────────
    // While a material is dragged over the view, live-preview it on the object
    // under the cursor (Unity-style), restoring the previous one when the cursor
    // moves off. The drop commits it (with undo); a cancel restores it.
    std::string PickObjectAt(const QPointF& viewPos);
    void UpdateMaterialPreview(const QPointF& viewPos);
    void RestoreMaterialPreview();

    std::string m_matDragId;        // material id currently being dragged ("" = none)
    std::string m_matPreviewObjId;  // GameObject showing the preview
    std::string m_matPreviewOrigId; // that object's material before the preview

    EditorRenderView* editorView = nullptr;   // borrowed; same object as ViewportWidget::view
    ImGuiInstance* imGuiInstance = nullptr;

    QPoint lastMousePos;
    bool isPanning = false;

    // ── Box (marquee) select ─────────────────────────────────────────────────
    // Right-drag box-selects, replacing the selection. Armed on press -- nothing
    // happens (rubber band included) until the drag clears Qt's own start-drag
    // distance, so a plain right-click (no movement) stays a deliberate no-op.
    //
    // Deliberately NOT on Shift+Left: deferring that click's pick/toggle to
    // mouseReleaseEvent (to leave room for a drag) made ordinary shift-click
    // selection feel laggy, so shift-click stays on the immediate press-time path.
    void PerformBoxSelect(const QRect& widgetRect);

    QRubberBand* m_boxSelectRubberBand = nullptr;
    QPoint m_boxSelectStart;
    bool m_boxSelectArmed = false;     // right-press seen, drag not yet confirmed
    bool m_boxSelecting = false;       // drag confirmed; rubber band is live
};
