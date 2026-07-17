#pragma once
#include "dock-widgets/ViewportWidget.hpp"   // must precede any Qt header that pulls GL headers (glad.h ordering)
#include <glm/glm.hpp>
#include <QWheelEvent>
#include <QMouseEvent>
#include "engine/rendering/views/GameRenderView.hpp"
#include "utils/ImGuiInstance.hpp"
#include "Engine.hpp"
using namespace EngineUtils;

class GameViewGui : public ViewportWidget {
    Q_OBJECT

public:
    static GameViewGui* Get() {
        static GameViewGui* instance = nullptr;
        if (!instance) {
            instance = new GameViewGui(nullptr);
        }
        return instance;
    }
    void Init();
    explicit GameViewGui(QWidget* parent = nullptr);

protected:
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
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // Unity-style aspect/resolution selector, drawn as an ImGui toolbar.
    void DrawToolBar();
    // Centered notice shown when no Camera is resolving for the Game view.
    void DrawNoCameraOverlay();
    void ApplyAspectPreset(int index);

    GameRenderView* gameView = nullptr;   // borrowed; same object as ViewportWidget::view
    ImGuiInstance* imGuiInstance = nullptr;
    int aspectPresetIndex = 0;            // index into kPresets (0 == Free Aspect)

    static inline Proxy<InputManager> inputManager;

    ~GameViewGui() override = default;
};
