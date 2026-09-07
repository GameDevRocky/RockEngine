#include "engine/bindings/PythonBindings.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/TrailRenderer.hpp"
#include "engine/utils/AnimationCurve.hpp"
#include "Engine.hpp"

// Keyed by the owning GameObject's id string, resolving through the global
// registry Proxy -- the same shape as every other component binding here.
//
// The one thing worth calling out is width_curve. AnimationCurve is bound as a
// real py::class_ (see AnimationCurveBindings.cpp), and it crosses this boundary
// BY VALUE. Reading the property hands Python a copy, so mutating that copy does
// not write back:
//
//     c = trail.width_curve
//     c.add_key(0.5, 0.8)
//     trail.width_curve = c     # <- required
//
// Same read-modify-write dance as Unity, and the alternative (handing out a
// pointer into a component that a scene unload can delete) is exactly what the
// id-keyed convention exists to avoid.
static TrailRenderer* Resolve(const std::string& id) {
    GameObject* go = registry->Find<GameObject>(id);
    return go ? go->GetComponent<TrailRenderer>() : nullptr;
}

void BindTrailRenderer(pybind11::module_& m) {
    pybind11::module_ pm = m.def_submodule("trail_module", "Trail Renderer Bindings");

    // ── Shape ──
    pm.def("get_time", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetTime();
        return 0.0f;
    });
    pm.def("set_time", [](const std::string& id, float v) {
        if (auto* t = Resolve(id)) t->SetTime(v);
    });

    pm.def("get_min_vertex_distance", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetMinVertexDistance();
        return 0.0f;
    });
    pm.def("set_min_vertex_distance", [](const std::string& id, float v) {
        if (auto* t = Resolve(id)) t->SetMinVertexDistance(v);
    });

    pm.def("get_width_curve", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetWidthCurve();
        return AnimationCurve();
    });
    pm.def("set_width_curve", [](const std::string& id, const AnimationCurve& c) {
        if (auto* t = Resolve(id)) t->SetWidthCurve(c);
    });

    pm.def("get_width_multiplier", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetWidthMultiplier();
        return 0.0f;
    });
    pm.def("set_width_multiplier", [](const std::string& id, float v) {
        if (auto* t = Resolve(id)) t->SetWidthMultiplier(v);
    });

    // ── Appearance (colours as 4-tuples, matching the other bindings) ──
    pm.def("get_start_color", [](const std::string& id) {
        if (auto* t = Resolve(id)) {
            glm::vec4 c = t->GetStartColor();
            return std::make_tuple(c.r, c.g, c.b, c.a);
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
    });
    pm.def("set_start_color", [](const std::string& id, float r, float g, float b, float a) {
        if (auto* t = Resolve(id)) t->SetStartColor(glm::vec4(r, g, b, a));
    });

    pm.def("get_end_color", [](const std::string& id) {
        if (auto* t = Resolve(id)) {
            glm::vec4 c = t->GetEndColor();
            return std::make_tuple(c.r, c.g, c.b, c.a);
        }
        return std::make_tuple(1.0f, 1.0f, 1.0f, 0.0f);
    });
    pm.def("set_end_color", [](const std::string& id, float r, float g, float b, float a) {
        if (auto* t = Resolve(id)) t->SetEndColor(glm::vec4(r, g, b, a));
    });

    pm.def("get_material", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetMaterialID();
        return std::string("");
    });
    pm.def("set_material", [](const std::string& id, const std::string& v) {
        if (auto* t = Resolve(id)) t->SetMaterial(v);
    });

    pm.def("get_sprite", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetSpriteID();
        return std::string("");
    });
    pm.def("set_sprite", [](const std::string& id, const std::string& v) {
        if (auto* t = Resolve(id)) t->SetSprite(v);
    });

    // ── Emission ──
    pm.def("get_emitting", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetEmitting();
        return false;
    });
    pm.def("set_emitting", [](const std::string& id, bool v) {
        if (auto* t = Resolve(id)) t->SetEmitting(v);
    });

    pm.def("get_autodestruct", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetAutodestruct();
        return false;
    });
    pm.def("set_autodestruct", [](const std::string& id, bool v) {
        if (auto* t = Resolve(id)) t->SetAutodestruct(v);
    });

    // ── Sorting ──
    pm.def("get_sorting_layer", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetSortingLayer();
        return std::string("Default");
    });
    pm.def("set_sorting_layer", [](const std::string& id, const std::string& v) {
        if (auto* t = Resolve(id)) t->SetSortingLayer(v);
    });

    pm.def("get_sorting_order", [](const std::string& id) {
        if (auto* t = Resolve(id)) return t->GetSortingOrder();
        return 0;
    });
    pm.def("set_sorting_order", [](const std::string& id, int v) {
        if (auto* t = Resolve(id)) t->SetSortingOrder(v);
    });

    // ── Actions / read-only state ──
    pm.def("clear", [](const std::string& id) {
        if (auto* t = Resolve(id)) t->Clear();
    });
    pm.def("get_point_count", [](const std::string& id) {
        if (auto* t = Resolve(id)) return static_cast<int>(t->GetPoints().size());
        return 0;
    });
}
