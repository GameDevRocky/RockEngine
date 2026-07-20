#pragma once
#include "dock-widgets/ViewportWidget.hpp"   // must precede any Qt header that pulls GL headers (glad.h ordering)
#include <QWheelEvent>
#include <QPoint>
#include <QMouseEvent>
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

protected:
    void DrawGizmos();
    void DrawToolBar();

    RenderView* CreateView(int pixelW, int pixelH) override;
    void OnViewInitialized() override;
    void OnResized(int logicalW, int logicalH) override;
    void OnAfterPresent() override;

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

    EditorRenderView* editorView = nullptr;   // borrowed; same object as ViewportWidget::view
    ImGuiInstance* imGuiInstance = nullptr;

    QPoint lastMousePos;
    bool isPanning = false;
};
