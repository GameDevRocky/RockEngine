#pragma once

#include <QPoint>
#include <Qt>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Container;
class EditorRenderView;
class ImGuiInstance;
class QEvent;

// Values supplied by the Scene view for one input or drawing call. Tools never
// need the widget itself, and screen coordinates remain logical Qt pixels.
struct SceneEditContext {
    EditorRenderView* view = nullptr;
    Container* container = nullptr;
    int width = 0;
    int height = 0;
    float dpi = 1.0f;
    Qt::KeyboardModifiers modifiers{};
    std::function<void()> requestRepaint;

    glm::vec2 ScreenToWorld(const QPoint& point) const;
};

struct SceneEditPointerEvent {
    QPoint position;
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons{};
    Qt::KeyboardModifiers modifiers{};
    bool inside = false;
};

// A tool owns its gesture from press through release. Passive previews may
// observe moves and draw even without a captured gesture.
class SceneEditTool {
public:
    virtual ~SceneEditTool() = default;
    virtual bool PointerPress(const SceneEditContext&, const SceneEditPointerEvent&) { return false; }
    virtual bool PointerMove(const SceneEditContext&, const SceneEditPointerEvent&) { return false; }
    virtual bool PointerRelease(const SceneEditContext&, const SceneEditPointerEvent&) { return false; }
    virtual bool KeyPress(const SceneEditContext&, int, Qt::KeyboardModifiers) { return false; }
    virtual void PointerLeave(const SceneEditContext&) {}
    virtual bool WantsCaptureMouse() const { return false; }
    virtual void Draw(const SceneEditContext&) {}
    virtual void Finish(const SceneEditContext&) {}
};

class SceneEditController {
public:
    SceneEditController();
    ~SceneEditController();
    void RegisterTool(std::unique_ptr<SceneEditTool> tool);

    // Called by the widget's event filter before Qt's handlers. It feeds this
    // view's ImGui context, then lets ImGui or a hovered gizmo claim the event.
    bool FilterImGuiEvent(ImGuiInstance* imgui, QEvent* event);
    bool PointerPress(const SceneEditContext&, const SceneEditPointerEvent&);
    bool PointerMove(const SceneEditContext&, const SceneEditPointerEvent&);
    bool PointerRelease(const SceneEditContext&, const SceneEditPointerEvent&);
    bool KeyPress(const SceneEditContext&, int key, Qt::KeyboardModifiers);
    void PointerLeave(const SceneEditContext&);
    void Draw(const SceneEditContext&);
    void Finish(const SceneEditContext&, ImGuiInstance* imgui = nullptr);

private:
    std::vector<std::unique_ptr<SceneEditTool>> tools;
    SceneEditTool* captured = nullptr;
    Qt::MouseButton capturedButton = Qt::NoButton;
};
