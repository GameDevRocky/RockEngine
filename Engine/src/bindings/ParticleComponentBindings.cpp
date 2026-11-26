#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ParticleComponent.hpp"
#include "Engine.hpp"

// All functions are keyed by the owning GameObject's id string (matching
// SpriteRenderer's bindings), resolving the emitter through the global registry
// Proxy. Enums cross the boundary as ints.
static ParticleComponent* Resolve(const std::string& id) {
    GameObject* go = registry->Find<GameObject>(id);
    return go ? go->GetComponent<ParticleComponent>() : nullptr;
}

void BindParticleComponent(pybind11::module_& m) {
    pybind11::module_ pm = m.def_submodule("particle_module", "Particle Component Bindings");

    // ── Emission ──
    pm.def("get_emission_rate", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetEmissionRate();
        return 0.0f;
    });
    pm.def("set_emission_rate", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetEmissionRate(v);
    });

    pm.def("get_max_particles", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetMaxParticles();
        return 0;
    });
    pm.def("set_max_particles", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetMaxParticles(v);
    });

    pm.def("get_duration", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetDuration();
        return 0.0f;
    });
    pm.def("set_duration", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetDuration(v);
    });

    pm.def("get_looping", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetLooping();
        return false;
    });
    pm.def("set_looping", [](const std::string& id, bool v) {
        if (auto* p = Resolve(id)) p->SetLooping(v);
    });

    pm.def("get_start_burst", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetStartBurst();
        return 0;
    });
    pm.def("set_start_burst", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetStartBurst(v);
    });

    // ── Lifetime / motion ── (min,max carried as 2-tuples)
    pm.def("get_lifetime", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto v = p->GetLifetime(); return std::make_tuple(v.x, v.y); }
        return std::make_tuple(0.0f, 0.0f);
    });
    pm.def("set_lifetime", [](const std::string& id, float mn, float mx) {
        if (auto* p = Resolve(id)) p->SetLifetime({mn, mx});
    });

    pm.def("get_speed", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto v = p->GetSpeed(); return std::make_tuple(v.x, v.y); }
        return std::make_tuple(0.0f, 0.0f);
    });
    pm.def("set_speed", [](const std::string& id, float mn, float mx) {
        if (auto* p = Resolve(id)) p->SetSpeed({mn, mx});
    });

    pm.def("get_direction", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetDirection();
        return 0.0f;
    });
    pm.def("set_direction", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetDirection(v);
    });

    pm.def("get_spread", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetSpread();
        return 0.0f;
    });
    pm.def("set_spread", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetSpread(v);
    });

    pm.def("get_gravity", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto v = p->GetGravity(); return std::make_tuple(v.x, v.y); }
        return std::make_tuple(0.0f, 0.0f);
    });
    pm.def("set_gravity", [](const std::string& id, float x, float y) {
        if (auto* p = Resolve(id)) p->SetGravity({x, y});
    });

    pm.def("get_damping", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetDamping();
        return 0.0f;
    });
    pm.def("set_damping", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetDamping(v);
    });

    pm.def("get_space", [](const std::string& id) {
        if (auto* p = Resolve(id)) return static_cast<int>(p->GetSpace());
        return 0;
    });
    pm.def("set_space", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetSpace(static_cast<ParticleComponent::SimulationSpace>(v));
    });

    // ── Shape ──
    pm.def("get_shape", [](const std::string& id) {
        if (auto* p = Resolve(id)) return static_cast<int>(p->GetShape());
        return 0;
    });
    pm.def("set_shape", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetShape(static_cast<ParticleComponent::Shape>(v));
    });

    pm.def("get_shape_size", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto v = p->GetShapeSize(); return std::make_tuple(v.x, v.y); }
        return std::make_tuple(0.0f, 0.0f);
    });
    pm.def("set_shape_size", [](const std::string& id, float x, float y) {
        if (auto* p = Resolve(id)) p->SetShapeSize({x, y});
    });

    pm.def("get_cone_angle", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetConeAngle();
        return 0.0f;
    });
    pm.def("set_cone_angle", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetConeAngle(v);
    });

    // ── Appearance ──
    pm.def("get_sprite_id", [](const std::string& id) -> std::string {
        if (auto* p = Resolve(id)) return p->GetSpriteID();
        return {};
    });
    pm.def("set_sprite_id", [](const std::string& id, const std::string& spriteId) {
        if (auto* p = Resolve(id)) p->SetSprite(spriteId);
    });

    pm.def("get_flip", [](const std::string& id) {
        if (auto* p = Resolve(id)) return std::make_tuple(p->GetFlipX(), p->GetFlipY());
        return std::make_tuple(false, false);
    });
    pm.def("set_flip", [](const std::string& id, bool x, bool y) {
        if (auto* p = Resolve(id)) { p->SetFlipX(x); p->SetFlipY(y); }
    });

    pm.def("get_start_color", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto c = p->GetStartColor(); return std::make_tuple(c.r, c.g, c.b, c.a); }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });
    pm.def("set_start_color", [](const std::string& id, float r, float g, float b, float a) {
        if (auto* p = Resolve(id)) p->SetStartColor({r, g, b, a});
    });

    pm.def("get_end_color", [](const std::string& id) {
        if (auto* p = Resolve(id)) { auto c = p->GetEndColor(); return std::make_tuple(c.r, c.g, c.b, c.a); }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 0.0f);
    });
    pm.def("set_end_color", [](const std::string& id, float r, float g, float b, float a) {
        if (auto* p = Resolve(id)) p->SetEndColor({r, g, b, a});
    });

    pm.def("get_start_size", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetStartSize();
        return 0.0f;
    });
    pm.def("set_start_size", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetStartSize(v);
    });

    pm.def("get_end_size", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetEndSize();
        return 0.0f;
    });
    pm.def("set_end_size", [](const std::string& id, float v) {
        if (auto* p = Resolve(id)) p->SetEndSize(v);
    });

    pm.def("get_blend_mode", [](const std::string& id) {
        if (auto* p = Resolve(id)) return static_cast<int>(p->GetBlendMode());
        return 0;
    });
    pm.def("set_blend_mode", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetBlendMode(static_cast<ParticleComponent::BlendMode>(v));
    });

    pm.def("get_sorting_layer", [](const std::string& id) -> std::string {
        if (auto* p = Resolve(id)) return p->GetSortingLayer();
        return {};
    });
    pm.def("set_sorting_layer", [](const std::string& id, const std::string& layer) {
        if (auto* p = Resolve(id)) p->SetSortingLayer(layer);
    });

    pm.def("get_sorting_order", [](const std::string& id) {
        if (auto* p = Resolve(id)) return p->GetSortingOrder();
        return 0;
    });
    pm.def("set_sorting_order", [](const std::string& id, int v) {
        if (auto* p = Resolve(id)) p->SetSortingOrder(v);
    });

    // ── One-shot burst (explosions / puffs) ──
    pm.def("emit_burst", [](const std::string& id, int count) {
        if (auto* p = Resolve(id)) p->EmitBurst(count);
    });
}
