#pragma once
#include <glad/glad.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QWheelEvent>
#include <QPoint>
#include <QMouseEvent>
#include <glm/glm.hpp>
#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"

class SceneViewGui : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    static SceneViewGui* Get() {
        static SceneViewGui* instance = nullptr;
        if (!instance) {
            instance = new SceneViewGui(nullptr);
        }
        return instance;
    }

    ~SceneViewGui() override;
    
    void Init();
    void Render();
    
    protected:

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    glm::vec2 ScreenToWorld(const QPoint& p);

    void initializeRenderPipeline();

    private:
    explicit SceneViewGui(QWidget* parent = nullptr);

    RenderPipeline* renderPipeline = nullptr;
    RenderCamera* camera = nullptr;

    
    QPoint lastMousePos;
    bool isPanning = false;
    
    QOpenGLShaderProgram program;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QElapsedTimer elapsedTimer;
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint quadShader = 0;
};
