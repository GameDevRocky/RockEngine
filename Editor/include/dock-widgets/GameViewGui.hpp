#pragma once
#include <QWidget>
#include <glad/glad.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <glm/glm.hpp>
#include "engine/rendering/pipelines/RenderPipeline.hpp"
#include "engine/rendering/cameras/RenderCamera.hpp"
#include <QElapsedTimer>

class GameViewGui : public QOpenGLWidget, protected QOpenGLFunctions {
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
    void Render();
    explicit GameViewGui(QWidget* parent = nullptr);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void initializeRenderPipeline();

private:
    RenderPipeline* renderPipeline = nullptr;
    RenderCamera* camera = nullptr;

    QOpenGLShaderProgram program;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QElapsedTimer elapsedTimer;
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint quadShader = 0;

    ~GameViewGui() override = default;


};
