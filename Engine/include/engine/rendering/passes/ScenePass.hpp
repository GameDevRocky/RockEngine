#pragma once
#include <glad/glad.h>
#include "engine/rendering/passes/RenderPass.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/debug/Console.hpp"
#include "engine/core/LayerManager.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_set>
#include "Engine.hpp"
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

// Draws every 2D renderable in the scene -- SpriteRenderers, ParticleComponents
// *and* TextRenderers -- in ONE list sorted by (sorting layer priority, order in
// layer). Particles and text share the sprite sorting model rather than
// compositing on top as separate passes, which is the only way an emitter or a
// label can sit behind or between sprite layers. Particle simulation is not done
// here; see ParticleSimulationPass, which must run before this pass.
class ScenePass : public RenderPass
{
public:
    ScenePass() = default;

    void Init() override;

    void Execute(RenderCamera* camera, Scene* scene) override ;

    void Shutdown() override ;

private:
    Proxy<LayerManager> layerManager;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    // Second VAO for the instanced particle draw, over ParticleManager's shared
    // quad VBO. The vertex data happens to be identical to this pass's own quad,
    // but binding the manager's buffer keeps the two independent -- a future edit
    // to either quad can't silently break the other. VAOs are per-context and so
    // cannot come from ParticleManager itself.
    unsigned int particleVao = 0;

    // Third VAO for glyph meshes. Set up with the separate attribute format
    // (glVertexAttribFormat/Binding) rather than glVertexAttribPointer, because
    // every TextRenderer owns its own VBO -- so the draw loop re-points the VAO
    // at each one with a single glBindVertexBuffer instead of re-specifying the
    // attribute layout per object.
    unsigned int textVao = 0;

    // Resolved once on first use; the material map only grows via asset loads,
    // so the pointer stays valid for the pass's lifetime (borrowed, not owned).
    Material* defaultMaterial = nullptr;
    Material* defaultTextMaterial = nullptr;

    // FontManager's mesh cache is GC'd once per frame, not once per scene --
    // this pass runs once per scene per view.
    std::uint64_t lastGcFrame = ~0ull;
    // Objects already warned about (missing transform/material) — warn once per
    // object instead of flooding the console every frame.
    std::unordered_set<const void*> warnedObjects;
};
