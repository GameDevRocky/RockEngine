#include "engine/rendering/Renderer.hpp"
#include <glad/glad.h>
#include <algorithm>
#include <iostream>

#include "engine/rendering/views/EditorRenderView.hpp"
#include "engine/rendering/views/GameRenderView.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/utils/EngineUtils.hpp"

Renderer& Renderer::Get()
{
    static Renderer instance;
    return instance;
}

bool Renderer::EnsureInitialized()
{
    if (initialized) return true;

    if (!gladLoadGL())
    {
        std::cerr << "Renderer: gladLoadGL failed" << std::endl;
        return false;
    }

    AssetManager::Get().Init();
    AssetManager::Get().Awake();

    const std::string domainDir = EngineUtils::GetAssetPath("Domain");
    AssetMetaService::ScanAndGenerate(domainDir);
    AssetManager::Get().LoadFromDirectory(domainDir);

    CreateBlitResources();

    initialized = true;
    return true;
}

void Renderer::CreateBlitResources()
{
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f
    };

    // Only the VBO (and the program below) are created here. AA_ShareOpenGLContexts
    // shares buffers and programs across every viewport's context, but a VAO is
    // container state that is NOT shared -- so the blit VAO is built per-call in
    // Blit(), in whatever context is current. (See the comment there.)
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const char* quadVert = R"(#version 460 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        TexCoord = aTexCoord;
        gl_Position = vec4(aPos, 1.0);
    })";

    const char* quadFrag = R"(#version 460 core
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D uTexture;
    void main() {
        FragColor = texture(uTexture, TexCoord);
    })";

    blitProgram = glCreateProgram();
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &quadVert, nullptr);
    glCompileShader(v);
    glShaderSource(f, 1, &quadFrag, nullptr);
    glCompileShader(f);
    glAttachShader(blitProgram, v);
    glAttachShader(blitProgram, f);
    glLinkProgram(blitProgram);
    glDeleteShader(v);
    glDeleteShader(f);
}

EditorRenderView* Renderer::CreateEditorView(int panelPixelW, int panelPixelH)
{
    auto* view = new EditorRenderView();
    view->Init();
    view->Resize(panelPixelW, panelPixelH);
    views.push_back(view);
    return view;
}

GameRenderView* Renderer::CreateGameView(int panelPixelW, int panelPixelH)
{
    auto* view = new GameRenderView();
    view->Init();
    view->Resize(panelPixelW, panelPixelH);
    views.push_back(view);
    return view;
}

void Renderer::DestroyView(RenderView* view)
{
    if (!view) return;
    auto it = std::find(views.begin(), views.end(), view);
    if (it != views.end()) views.erase(it);
    delete view;
}

void Renderer::Blit(unsigned int texture, unsigned int targetFBO, int x, int y, int w, int h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(x, y, w, h);
    glUseProgram(blitProgram);

    // A VAO is per-context state; unlike the VBO/program it is NOT shared by
    // AA_ShareOpenGLContexts. Each viewport (Scene, Game) blits from its own
    // context, so a single VAO created once in whichever context booted first
    // would, in the other context, alias an unrelated VAO of the same id --
    // typically ScenePass's -0.5..0.5 unit quad -- shrinking the "fullscreen"
    // blit to the centered half of the panel (gray image, black borders).
    // Build a transient VAO over the shared VBO each call so the vertex state
    // is always valid in the current context. Two blits per frame: negligible.
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(blitProgram, "uTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
}
