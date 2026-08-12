#include "engine/rendering/passes/ScenePass.hpp"
#include "engine/rendering/core/ParticleManager.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/rendering/core/FontManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "engine/components/TextRenderer.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/debug/FrameProfiler.hpp"
#include <algorithm>
#include <vector>

void ScenePass::Init(){
    float quadVerts[] =
    {
        // pos      // uv
        -0.5f, -0.5f, 0.0f, 0.0f,
            0.5f, -0.5f, 1.0f, 0.0f,
            0.5f,  0.5f, 1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f, 0.0f,
            0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glad_glGenVertexArrays(1, &vao);
    glad_glGenBuffers(1, &vbo);

    glad_glBindVertexArray(vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glad_glEnableVertexAttribArray(0); // pos
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glad_glEnableVertexAttribArray(1); // uv
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glad_glBindVertexArray(0);

    // Particle draws go through this pass too (see the class comment), so make
    // sure the manager's programs + shared quad exist and build a VAO over them.
    ParticleManager& pm = ParticleManager::Get();
    pm.EnsureInitialized();

    glad_glGenVertexArrays(1, &particleVao);
    glad_glBindVertexArray(particleVao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pm.GetQuadVBO());
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glBindVertexArray(0);
    glad_glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Glyph meshes live one-VBO-per-TextRenderer in FontManager, so this VAO
    // describes the layout without naming a buffer. Each draw supplies its own
    // via glBindVertexBuffer against binding point 0. Same interleaved
    // { vec2 pos, vec2 uv } as the sprite quad above.
    glad_glGenVertexArrays(1, &textVao);
    glad_glBindVertexArray(textVao);
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glad_glVertexAttribBinding(0, 0);
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glad_glVertexAttribBinding(1, 0);
    glad_glBindVertexArray(0);
}

void ScenePass::Execute(RenderCamera* camera, Scene* scene)
{
    ROCK_PROFILE_SCOPE("ScenePass");
    if (!scene) return;
    const auto& objects = scene->GetAllGameObjects();

    // One entry per renderable. Exactly one of `renderer` / `emitter` / `text` is
    // set -- sprites, particle emitters and text sort against each other in a
    // single list, so any of them can sit among the sprites rather than on top.
    struct DrawCall
    {
        Transform*         transform      = nullptr;
        SpriteRenderer*    renderer       = nullptr;   // sprite path
        Material*          mat            = nullptr;
        Shader*            shader         = nullptr;
        ParticleComponent* emitter        = nullptr;   // particle path
        TextRenderer*      text           = nullptr;   // text path
        Font*              font           = nullptr;
        int                layerPriority  = 0;
        int                sortingOrder   = 0;
    };

    std::vector<DrawCall> drawCalls;
    drawCalls.reserve(objects.size());

    for (auto* obj : objects)
    {
        if (!obj || !obj->GetActive()) continue;

        Transform* transform = obj->GetComponent<Transform>();
        if (!transform)
        {
            // Only worth warning about for something that wanted to draw.
            if ((obj->GetComponent<SpriteRenderer>() || obj->GetComponent<ParticleComponent>())
                && warnedObjects.insert(obj).second)
                Console::Alert("No Loaded Transform");
            continue;
        }

        if (SpriteRenderer* renderer = obj->GetComponent<SpriteRenderer>())
        {
            if (renderer->GetEnabled() && renderer->GetVisible()
                && (!camera || camera->PassesCullingMask(renderer->GetSortingLayer())))
            {
                Material* mat = renderer->GetMaterial();
                if (!mat)
                {
                    if (!defaultMaterial)
                        defaultMaterial = AssetManager::Get().GetMaterialByName("default");
                    mat = defaultMaterial;
                    if (warnedObjects.insert(obj).second)
                        Console::Alert("Assigning Default Material to " + transform->GetGameObject()->GetName());
                }
                if (mat && mat->GetShader())
                {
                    DrawCall dc;
                    dc.transform = transform;
                    dc.renderer  = renderer;
                    dc.mat       = mat;
                    dc.shader    = mat->GetShader();
                    dc.layerPriority = layerManager ? layerManager->GetPriority(renderer->GetSortingLayer()) : 0;
                    dc.sortingOrder  = renderer->GetSortingOrder();
                    drawCalls.push_back(dc);
                }
            }
        }

        if (TextRenderer* text = obj->GetComponent<TextRenderer>())
        {
            if (text->GetEnabled() && text->GetVisible()
                && (!camera || camera->PassesCullingMask(text->GetSortingLayer())))
            {
                Font* font = text->GetFont();
                // The bake + upload happens here, on first use, because this is
                // the first point in the frame with a guaranteed current context.
                // No-op once the atlas exists. See Font::EnsureUploaded.
                if (font) font->EnsureUploaded();

                Material* mat = text->GetMaterial();
                if (!mat)
                {
                    if (!defaultTextMaterial)
                        defaultTextMaterial = AssetManager::Get().GetMaterialByName("text");
                    mat = defaultTextMaterial;
                    if (mat && warnedObjects.insert(text).second)
                        Console::Alert("Assigning Default Text Material to " + obj->GetName());
                }

                if (font && font->IsReady() && mat && mat->GetShader())
                {
                    DrawCall dc;
                    dc.transform = transform;
                    dc.text      = text;
                    dc.font      = font;
                    dc.mat       = mat;
                    dc.shader    = mat->GetShader();
                    dc.layerPriority = layerManager ? layerManager->GetPriority(text->GetSortingLayer()) : 0;
                    dc.sortingOrder  = text->GetSortingOrder();
                    drawCalls.push_back(dc);
                }
            }
        }

        // An object can carry both a sprite and an emitter; each is its own entry
        // and sorts on its own layer/order.
        if (ParticleComponent* emitter = obj->GetComponent<ParticleComponent>())
        {
            if (emitter->GetEnabled()
                && (!camera || camera->PassesCullingMask(emitter->GetSortingLayer())))
            {
                DrawCall dc;
                dc.transform = transform;
                dc.emitter   = emitter;
                dc.layerPriority = layerManager ? layerManager->GetPriority(emitter->GetSortingLayer()) : 0;
                dc.sortingOrder  = emitter->GetSortingOrder();
                drawCalls.push_back(dc);
            }
        }
    }

    // Stable, so a full tie on every key falls back to scene scan order rather
    // than an unspecified one. PickingPass replays this exact comparison to
    // decide what a click resolves to, and only a deterministic tiebreak keeps
    // the two from disagreeing about which of two identical sprites is on top.
    std::stable_sort(drawCalls.begin(), drawCalls.end(), [](const DrawCall& a, const DrawCall& b)
    {
        if (a.layerPriority != b.layerPriority) return a.layerPriority < b.layerPriority;
        if (a.sortingOrder  != b.sortingOrder)  return a.sortingOrder  < b.sortingOrder;
        // Within a layer+order tie, order by kind so the result is deterministic:
        // sprites, then text, then particles. Text sits above sprites because a
        // label tied with the art it annotates is meant to be readable over it;
        // particles stay last because they are usually the effect on top of
        // everything. Then group by shader to cut program rebinds.
        auto kindOf = [](const DrawCall& d) {
            return d.emitter ? 2 : (d.text ? 1 : 0);
        };
        const int ka = kindOf(a), kb = kindOf(b);
        if (ka != kb) return ka < kb;
        if (a.shader && b.shader) return a.shader->GetProgramID() < b.shader->GetProgramID();
        return false;
    });

    const glm::mat4& viewMatrix = camera->GetViewMatrix();
    const glm::mat4& projMatrix = camera->GetProjectionMatrix();

    // Stamps FontManager's mesh cache so it can drop buffers for TextRenderers
    // that stopped being drawn. Falls back to a value that never matches the GC
    // sweep below, so a missing TimeManager degrades to "never collect" rather
    // than "collect everything every frame".
    Container* activeContainer = Engine::Get()->GetActiveContainer();
    TimeManager* timeManager = activeContainer ? activeContainer->FindSystem<TimeManager>() : nullptr;
    const std::uint64_t frameId = timeManager ? timeManager->FrameCount() : 0ull;

    ParticleManager& pm = ParticleManager::Get();
    glad_glBindVertexArray(vao);
    GLuint lastProgramID = 0;
    bool particleStateActive = false;   // depth-mask/blend left in particle config

    for (const DrawCall& dc : drawCalls)
    {
        if (dc.emitter)
        {
            // Particles don't occlude, and each emitter picks its own blend mode.
            if (!particleStateActive)
            {
                glad_glDepthMask(GL_FALSE);
                particleStateActive = true;
            }
            if (dc.emitter->GetBlendMode() == ParticleComponent::BlendMode::Additive)
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Local-space emitters draw through the emitter world matrix; World-
            // space particles already carry world coords (identity).
            glm::mat4 model(1.0f);
            if (dc.emitter->GetSpace() == ParticleComponent::SimulationSpace::Local)
                model = dc.transform->GetWorldMatrix();

            // Resolve the sprite to a texture AND its UV sub-rect: a sprite is a
            // region of a (possibly atlas) texture, so sampling the raw texture
            // over [0,1] would show the whole sheet instead of the sprite. Flip is
            // folded into the scale's sign about the sprite's own rect, so a
            // flipped atlas sprite stays inside its region -- same convention as
            // SpriteRenderer::OverrideUniforms.
            unsigned int texId = 0;
            glm::vec2 uvScale(1.0f, 1.0f);
            glm::vec2 uvOffset(0.0f, 0.0f);
            if (!dc.emitter->GetSpriteID().empty()) {
                if (Sprite* sprite = AssetManager::Get().GetSprite(dc.emitter->GetSpriteID())) {
                    if (Texture2D* tex = sprite->GetTexture()) {
                        texId = tex->GetTextureID();
                        uvScale  = sprite->GetUVMax() - sprite->GetUVMin();
                        uvOffset = sprite->GetUVMin();
                        if (dc.emitter->GetFlipX()) {
                            uvScale.x  = -uvScale.x;
                            uvOffset.x = sprite->GetUVMax().x;
                        }
                        if (dc.emitter->GetFlipY()) {
                            uvScale.y  = -uvScale.y;
                            uvOffset.y = sprite->GetUVMax().y;
                        }
                    }
                }
            }

            // Draw() owns its program and unbinds the VAO it was handed, so the
            // sprite path must re-bind both afterwards.
            pm.Draw(dc.emitter, viewMatrix, projMatrix, model, texId,
                    uvScale, uvOffset, particleVao);
            glad_glBindVertexArray(vao);
            lastProgramID = 0;
            continue;
        }

        if (particleStateActive)
        {
            glad_glDepthMask(GL_TRUE);
            glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            particleStateActive = false;
        }

        if (dc.shader->GetProgramID() != lastProgramID)
        {
            dc.shader->Bind();
            dc.shader->SetMat4("uView", viewMatrix);
            dc.shader->SetMat4("uProj", projMatrix);
            lastProgramID = dc.shader->GetProgramID();
        }

        if (dc.text)
        {
            const int vertexCount = FontManager::Get().EnsureMesh(dc.text, dc.font, frameId);
            if (vertexCount > 0)
            {
                dc.shader->SetMat4("uModel", dc.transform->GetWorldMatrix());

                // Engine-bound, on a slot high enough that a material adding one
                // more sampler tomorrow cannot land on it. Bound BEFORE
                // ApplyUniforms so the material's own samplers (which allocate
                // upward from 0) can never collide.
                glad_glActiveTexture(GL_TEXTURE0 + TextureSlots::FontAtlas);
                glad_glBindTexture(GL_TEXTURE_2D, dc.font->GetAtlasTextureID());
                dc.shader->SetTexture("uMSDF", TextureSlots::FontAtlas);
                dc.shader->SetFloat("uPxRange", dc.font->GetPxRange());

                const int nextSlot = dc.mat->ApplyUniforms();
                dc.text->OverrideUniforms(nextSlot);

                glad_glBindVertexArray(textVao);
                glad_glBindVertexBuffer(0, FontManager::Get().GetVBO(dc.text->GetID()), 0,
                                        FontManager::kFloatsPerVertex * sizeof(float));

                // Glyph edges are antialiased alpha, so they must not write
                // depth -- a half-transparent edge texel would otherwise occlude
                // whatever is drawn after it at the same depth, leaving a visible
                // fringe around every character.
                glad_glDepthMask(GL_FALSE);
                glad_glDrawArrays(GL_TRIANGLES, 0, vertexCount);
                glad_glDepthMask(GL_TRUE);

                // Hand the sprite quad's VAO back, since the loop's other branch
                // assumes it is still bound.
                glad_glBindVertexArray(vao);
            }
            continue;
        }

        dc.shader->SetMat4("uModel", dc.transform->GetWorldMatrix());
        const int nextSlot = dc.mat->ApplyUniforms();
        dc.renderer->OverrideUniforms(nextSlot);

        glad_glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    if (particleStateActive)
    {
        glad_glDepthMask(GL_TRUE);
        glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glad_glBindVertexArray(0);
    glad_glUseProgram(0);

    // Once per frame, not once per scene or per view -- this pass runs for every
    // scene of every viewport, and sweeping on each of those would delete the
    // meshes the next call is about to redraw.
    if (frameId != lastGcFrame) {
        FontManager::Get().GarbageCollect(frameId);
        lastGcFrame = frameId;
    }
}

void ScenePass::Shutdown()
{
    if (vbo) glad_glDeleteBuffers(1, &vbo);
    if (vao) glad_glDeleteVertexArrays(1, &vao);
    // particleVao and textVao are ours (VAOs are per-context); the buffers they
    // reference belong to ParticleManager and FontManager and must not be
    // deleted here.
    if (particleVao) glad_glDeleteVertexArrays(1, &particleVao);
    if (textVao) glad_glDeleteVertexArrays(1, &textVao);
    vao = vbo = particleVao = textVao = 0;
}
