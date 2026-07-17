#pragma once

#include <vector>

class RenderView;
class EditorRenderView;
class GameRenderView;

// Process-global graphics state. Lives OUTSIDE any Container, alongside
// AssetManager/GizmosManager: render resources have no per-world identity,
// so this must survive the editor/runtime container swap untouched.
//
// Owns every RenderView it hands out, the shared fullscreen-quad blit
// resources, and does the one-time glad/AssetManager bootstrap that used to
// be buried in SceneViewGui::initializeGL.
class Renderer
{
public:
    static Renderer& Get();

    // Call from a GL-context-current scope (a QOpenGLWidget's initializeGL).
    // Idempotent: gladLoadGL, the AssetManager bootstrap, and the blit
    // resources are all created exactly once, whichever viewport gets a
    // context first. Requires AA_ShareOpenGLContexts (set in Editor::Init)
    // so every other viewport's context can see what gets created here.
    bool EnsureInitialized();
    bool IsInitialized() const { return initialized; }

    // Context must be current. The returned view is owned by Renderer.
    EditorRenderView* CreateEditorView(int panelPixelW, int panelPixelH);
    GameRenderView*   CreateGameView(int panelPixelW, int panelPixelH);

    // Context must be current.
    void DestroyView(RenderView* view);

    // Draws `texture` into `targetFBO` at the pixel rect (x, y, w, h),
    // GL bottom-left origin. Owns the only fullscreen quad + blit program.
    void Blit(unsigned int texture, unsigned int targetFBO, int x, int y, int w, int h);

private:
    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void CreateBlitResources();

    bool initialized = false;
    std::vector<RenderView*> views;

    unsigned int quadVBO = 0;       // shared across contexts; the blit VAO is per-call (see Blit)
    unsigned int blitProgram = 0;
};
