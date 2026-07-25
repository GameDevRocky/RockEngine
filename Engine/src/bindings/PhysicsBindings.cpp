#include "engine/bindings/PythonBindings.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/utils/EngineUtils.hpp"
#include <iostream>
#include "Engine.hpp"

namespace py = pybind11;
using namespace EngineUtils::RenderUtils;

void BindPhysics(py::module_& m) {
    py::module_ physics_module = m.def_submodule("physics_module", "Physics Bindings");
    
    physics_module.def("cast_ray", [](py::object py_origin, py::object py_direction) -> py::object {
        // Convert from pixels (world units) to Box2D meters
        b2Vec2 origin = { PixelsToMeters(py_origin.attr("x").cast<float>()),
                          PixelsToMeters(py_origin.attr("y").cast<float>()) };
        b2Vec2 translation = { PixelsToMeters(py_direction.attr("x").cast<float>()),
                               PixelsToMeters(py_direction.attr("y").cast<float>()) };

        b2RayResult result = physicsSystem->CastRay(origin, translation, b2DefaultQueryFilter());

        if (!result.hit) {
            return py::none();
        }

        b2BodyId bodyId = b2Shape_GetBody(result.shapeId);
        GameObject* hitObj = static_cast<GameObject*>(b2Body_GetUserData(bodyId));

        // Convert hit point back to pixels (world units)
        py::dict dict;
        dict["hit"] = true;
        dict["point"] = py::make_tuple(MetersToPixels(result.point.x), MetersToPixels(result.point.y));
        dict["normal"] = py::make_tuple(result.normal.x, result.normal.y);
        dict["fraction"] = result.fraction;
        dict["game_object_id"] = hitObj ? hitObj->GetID() : "";

        return dict;
    });
}