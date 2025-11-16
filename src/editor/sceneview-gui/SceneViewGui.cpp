#include "SceneViewGui.hpp"
#include "engine/rendering/Renderer.hpp" 
#include <QDebug>
#include "engine/debug/Console.hpp"


SceneViewGui::SceneViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
    frameTimer.setInterval(16);
    connect(&frameTimer, &QTimer::timeout, this, [this]() {
        this->update();
    });
    frameTimer.start();
}


SceneViewGui::~SceneViewGui()
{
    makeCurrent();
    Renderer::Get().Shutdown();
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
    
    Renderer::Get().Render();
   
}
void SceneViewGui::Init(){
    
}