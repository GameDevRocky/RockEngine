#include "dock-widgets/ViewportWidget.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/rendering/views/RenderView.hpp"
#include <QSurfaceFormat>
#include <iostream>

ViewportWidget::ViewportWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
}

ViewportWidget::~ViewportWidget()
{
    // Dead in practice today: SceneViewGui/GameViewGui are leaked
    // function-local-static singletons (see their Get()), so this destructor
    // never actually runs during normal operation. Implemented correctly
    // anyway for when that changes -- Renderer::DestroyView requires a
    // current context, which makeCurrent()/doneCurrent() here guarantees.
    if (view)
    {
        makeCurrent();
        Renderer::Get().DestroyView(view);
        doneCurrent();
        view = nullptr;
    }
}

int ViewportWidget::PixelWidth() const  { return static_cast<int>(width()  * devicePixelRatioF()); }
int ViewportWidget::PixelHeight() const { return static_cast<int>(height() * devicePixelRatioF()); }

void ViewportWidget::initializeGL()
{
    initializeOpenGLFunctions();

    if (!Renderer::Get().EnsureInitialized())
    {
        QSurfaceFormat fmt = context()->format();
        std::cerr << "ViewportWidget: Renderer::EnsureInitialized failed. Context obtained: "
                   << fmt.majorVersion() << "." << fmt.minorVersion()
                   << (fmt.profile() == QSurfaceFormat::CoreProfile ? " core" : " non-core")
                   << " | GL_VENDOR=" << reinterpret_cast<const char*>(glGetString(GL_VENDOR))
                   << " | GL_RENDERER=" << reinterpret_cast<const char*>(glGetString(GL_RENDERER))
                   << " | GL_VERSION=" << reinterpret_cast<const char*>(glGetString(GL_VERSION))
                   << std::endl;
        return;
    }

    view = CreateView(PixelWidth(), PixelHeight());
    OnViewInitialized();
}

void ViewportWidget::resizeGL(int w, int h)
{
    if (!view) return;   // EnsureInitialized failed in initializeGL -- nothing to resize
    view->Resize(PixelWidth(), PixelHeight());
    OnResized(w, h);
}

void ViewportWidget::paintGL()
{
    if (!view) return;
    makeCurrent();
    // Re-assert the panel size every frame, not just in resizeGL(): a tab that
    // starts hidden in a QTabWidget can be laid out to its final on-screen size
    // without a matching resizeGL for that change (Qt defers layout/paint for
    // non-current tab pages). Without this, RenderView (via ApplyTargetSizing)
    // sizes its render target off a stale panelW/panelH -- smaller than the
    // PixelWidth()/PixelHeight() Present() clears below -- so the image ends up
    // mis-sized and floating in black bars. Cheap: two int stores unless the
    // size actually changed, and it keeps Resize and Present in lockstep on the
    // exact same values within a frame.
    view->Resize(PixelWidth(), PixelHeight());
    view->Render();
    view->Present(static_cast<unsigned int>(defaultFramebufferObject()), PixelWidth(), PixelHeight());
    OnAfterPresent();
}
