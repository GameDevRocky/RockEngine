#include "SceneViewGui.hpp"
#include <QOpenGLShader>
#include <QDebug>

SceneViewGui::SceneViewGui(QWidget* parent)
    : QOpenGLWidget(parent)
{
    frameTimer.setInterval(16);
    connect(&frameTimer, &QTimer::timeout, this, QOverload<>::of(&SceneViewGui::update));
}

SceneViewGui::~SceneViewGui()
{
    makeCurrent();
    vao.destroy();
    vbo.destroy();
    program.release();
    doneCurrent();
}

void SceneViewGui::Init()
{
    // any extra initialization
}

void SceneViewGui::initializeGL()
{
    initializeOpenGLFunctions();

    // Vertex shader: pass position through
    const char* vsrc =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    // Fragment shader: color modulated by time uniform
    const char* fsrc =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform float u_time;\n"
        "void main() {\n"
        "    // create smooth color cycling\n"
        "    float r = 0.5 + 0.5 * sin(u_time + 0.0);\n"
        "    float g = 0.5 + 0.5 * sin(u_time + 2.094); // +120 degrees\n"
        "    float b = 0.5 + 0.5 * sin(u_time + 4.188); // +240 degrees\n"
        "    FragColor = vec4(r, g, b, 1.0);\n"
        "}\n";

    if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) {
        qWarning() << "Vertex shader compile error:" << program.log();
    }
    if (!program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc)) {
        qWarning() << "Fragment shader compile error:" << program.log();
    }
    if (!program.link()) {
        qWarning() << "Shader link error:" << program.log();
    }

    // Triangle vertex data (NDC coordinates)
    const GLfloat vertices[] = {
        0.0f,  0.6f,   // top
       -0.6f, -0.6f,   // bottom left
        0.6f, -0.6f    // bottom right
    };

    // Setup VBO + VAO
    vao.create();
    vao.bind();

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    program.bind();
    const int posLoc = 0; // matches layout(location = 0)
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), reinterpret_cast<void*>(0));

    vbo.release();
    vao.release();
    program.release();

    // start timers
    elapsedTimer.start();
    frameTimer.start();

    // OpenGL state
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void SceneViewGui::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void SceneViewGui::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    program.bind();
    float t = elapsedTimer.elapsed() / 1000.0f; // seconds
    program.setUniformValue("u_time", t);

    vao.bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    vao.release();

    program.release();
}
