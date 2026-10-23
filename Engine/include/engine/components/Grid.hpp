#pragma once
#include <iostream>
#include "engine/components/Component.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>

class Grid : public Component{

    public:

    static inline const Event SIZE_CHANGED_EVENT = Grid::CreateEvent();

    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override {};
    void PostInit() override {};

    Grid* Copy() override;
    void Accept(IVisitor* v) override;

    std::string GetTypeName() const override {return "Grid";}


    glm::vec2 GetSize(){return cellSize;}

    void SetSize(glm::vec2 size);
    


    private:
        glm::vec2 cellSize = {1.0f,1.0f};
        float cellWidth = 32.0f;
        float cellHeight = 32.0f;


};