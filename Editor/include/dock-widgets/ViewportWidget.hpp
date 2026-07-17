#pragma once
#include <glad/glad.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class RenderView;

// Hosts a RenderView in a Qt GL context. Knows nothing about pipelines,
// passes, blits, or glad -- it hands the engine an FBO id and a pixel size.
// This is the entire ~110-line duplicated block that used to live separately
// in SceneViewGui and GameViewGui's initializeGL/resizeGL/paintGL, in one
// place. Subclasses provide the RenderView type and any widget-specific
// per-frame work (ImGui overlay, gizmos) via the Onxxx hooks.
class ViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

protected:
    // Called once, with a current GL context and Renderer already
    // initialized (glad loaded, AssetManager bootstrapped, blit resources
    // ready). Must construct and return this widget's RenderView via
    // Renderer::Get().CreateEditorView()/CreateGameView().
    virtual RenderView* CreateView(int pixelW, int pixelH) = 0;

    virtual void OnViewInitialized() {}          // e.g. SceneView builds its ImGui overlay here
    virtual void OnResized(int logicalW, int logicalH) {}   // e.g. SceneView resizes ImGui here
    virtual void OnAfterPresent() {}              // e.g. SceneView renders ImGui on top here

    void initializeGL() override final;
    void resizeGL(int w, int h) override final;
    void paintGL() override final;

    int PixelWidth() const;    // width()  * devicePixelRatioF(), DPI-scaled framebuffer size
    int PixelHeight() const;   // height() * devicePixelRatioF()

    RenderView* view = nullptr;   // borrowed; Renderer owns it
};
