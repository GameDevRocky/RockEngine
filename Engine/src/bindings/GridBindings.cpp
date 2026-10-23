#include "engine/bindings/PythonBindings.hpp"
#include "engine/components/Grid.hpp"
#include <iostream>
#include "Engine.hpp"


void BindGrid(pybind11::module_ & m){

    pybind11::module_ grid_module = m.def_submodule("grid_module", "Grid Bindings");
    grid_module.def("get_size", [](const std::string& go_id){

        auto* obj = registry->Find<GameObject>(go_id);
        if (obj){
            auto* grid = obj->GetComponent<Grid>();
            const glm::vec2& size = grid->GetSize();
            return std::make_tuple(size.x, size.y);
        }
        return std::make_tuple(0.0f, 0.0f);
    });

    grid_module.def("set_size", [](const std::string& go_id, float x, float y){

        auto* obj = registry->Find<GameObject>(go_id);
        if (obj){
            auto* grid = obj->GetComponent<Grid>();
            grid->SetSize({x, y});
        }
    });


}