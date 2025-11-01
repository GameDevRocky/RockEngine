#include "SceneViewGui.hpp"
#include "engine/rendering/Renderer.hpp" // your singleton renderer
#include <QDebug>

SceneViewGui::SceneViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
    frameTimer.setInterval(16); // ~60 FPS
    connect(&frameTimer, &QTimer::timeout, this, QOverload<>::of(&SceneViewGui::update));
}

SceneViewGui::~SceneViewGui()
{
    makeCurrent();
    Renderer::Get().Shutdown(); // cleanup GL resources
    doneCurrent();
}
void SceneViewGui::initializeGL() {
    initializeOpenGLFunctions();
    Renderer::Get().Init();
    Renderer::Get().CreateFramebuffer(width(), height());
}

void SceneViewGui::resizeGL(int w, int h) {
    Renderer::Get().ResizeFramebuffer(w, h);
    glViewport(0, 0, w, h);
}

void SceneViewGui::paintGL() {
    Renderer::Get().Clear(0.1f, 0.1f, 0.1f, 1.0f);
    Renderer::Get().Render();
}
