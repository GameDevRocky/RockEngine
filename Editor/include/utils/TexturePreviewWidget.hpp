#pragma once
#include <glad/glad.h>
#include <QOpenGLWidget>
#include <string>

// A small live GL preview of a Texture2D, drawn with the engine's own sprite
// shader into the shared context. It samples the real GPU texture, so it shows
// the texture as the engine actually has it uploaded — including the effect of
// the Filtering/Wrap settings — rather than a re-decoded copy of the source file.
//
// Holds the asset id rather than the Texture2D*: the texture can be destroyed
// while the inspector still shows it (Texture2D fires DESTROYED_EVENT for exactly
// this reason), and an id is re-resolved per paint instead of dangling.
class TexturePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit TexturePreviewWidget(const std::string& textureId, QWidget* parent = nullptr);
    ~TexturePreviewWidget() override;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    std::string m_textureId;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
};
