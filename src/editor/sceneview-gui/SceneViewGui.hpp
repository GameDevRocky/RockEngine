#pragma once
#include <glad/glad.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <QTimer>

class SceneViewGui : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    static SceneViewGui* Get() {
        static SceneViewGui* instance = new SceneViewGui(nullptr);
        return instance;
    }

    explicit SceneViewGui(QWidget* parent = nullptr);
    ~SceneViewGui() override;

    void Init();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram program;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QElapsedTimer elapsedTimer;
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint quadShader = 0;
};
