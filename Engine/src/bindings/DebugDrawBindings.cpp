#include "engine/bindings/PythonBindings.hpp"
#include "engine/debug/DebugDrawManager.hpp"

namespace py = pybind11;

void BindDebugDraw(py::module_& m)
{
    py::module_ debug_module = m.def_submodule("debug_draw_module", "Debug Draw Bindings");

    debug_module.def("draw_point", [](py::object pos, py::tuple color)
    {
        glm::vec2 p = { pos.attr("x").cast<float>(), pos.attr("y").cast<float>() };
        glm::vec4 c = { color[0].cast<float>(), color[1].cast<float>(),
                         color[2].cast<float>(), color[3].cast<float>() };
        DebugDrawManager::Get()->DrawPoint(p, c);
    });

    debug_module.def("draw_line", [](py::object start, py::object end, py::tuple color)
    {
        glm::vec2 a = { start.attr("x").cast<float>(), start.attr("y").cast<float>() };
        glm::vec2 b = { end.attr("x").cast<float>(),   end.attr("y").cast<float>() };
        glm::vec4 c = { color[0].cast<float>(), color[1].cast<float>(),
                         color[2].cast<float>(), color[3].cast<float>() };
        DebugDrawManager::Get()->DrawLine(a, b, c);
    });

    debug_module.def("draw_box", [](py::object center, py::object size, float rotation, py::tuple color)
    {
        glm::vec2 c2  = { center.attr("x").cast<float>(), center.attr("y").cast<float>() };
        glm::vec2 s   = { size.attr("x").cast<float>(),   size.attr("y").cast<float>() };
        glm::vec4 col = { color[0].cast<float>(), color[1].cast<float>(),
                           color[2].cast<float>(), color[3].cast<float>() };
        DebugDrawManager::Get()->DrawBox(c2, s, rotation, col);
    });

    debug_module.def("draw_circle", [](py::object center, float radius, py::tuple color)
    {
        glm::vec2 c2  = { center.attr("x").cast<float>(), center.attr("y").cast<float>() };
        glm::vec4 col = { color[0].cast<float>(), color[1].cast<float>(),
                           color[2].cast<float>(), color[3].cast<float>() };
        DebugDrawManager::Get()->DrawCircle(c2, radius, col);
    });
}
