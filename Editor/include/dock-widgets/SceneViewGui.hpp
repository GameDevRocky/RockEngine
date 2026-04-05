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
#include "engine/rendering/passes/PickingPass.hpp"
#include "utils/ImGuiInstance.hpp"

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
    void DrawGizmos();
    void DrawToolBar();
    void initializeRenderPipeline();

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    glm::vec2 ScreenToWorld(const QPoint& p);


    private:
    explicit SceneViewGui(QWidget* parent = nullptr);

    RenderPipeline* renderPipeline = nullptr;
    RenderCamera* camera = nullptr;
    PickingPass* pickingPass = nullptr;
    ImGuiInstance* imGuiInstance = nullptr;
    ImGuizmo::OPERATION m_currentOperation = ImGuizmo::TRANSLATE;

    
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
