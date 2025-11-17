#pragma once
#include <string>
#include <glad/glad.h>

class Texture2D {
public:
    Texture2D() = default;
    Texture2D(const std::string& path, bool flip = true);

    ~Texture2D();

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    GLuint GetID() const { return m_ID; }

private:
    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
};
