#pragma once
#include <iostream>
#include "engine/components/Component.hpp"
#include "yaml-cpp/yaml.h"
#include <glm/glm.hpp>

class Grid : public Component{

    public:

    static inline const Event SIZE_CHANGED_EVENT = Grid::CreateEvent();
    static inline const Event GAP_CHANGED_EVENT = Grid::CreateEvent();

    void Deserialize(const YAML::Node& node) override;
    YAML::Node Serialize() override;

    void Awake() override {};
    void PostInit() override {};

    Grid* Copy() override;
    void Accept(IVisitor* v) override;

    std::string GetTypeName() const override {return "Grid";}


    glm::vec2 GetSize(){return cellSize;}
    glm::vec2 GetGap(){return gapSize;}

    void SetSize(glm::vec2 size);
    void SetGap(glm::vec2 gap);

    glm::vec2 CellToWorld(const glm::vec2& pos);
    glm::ivec2 WorldToCell(const glm::vec2& pos);
    


    private:
        glm::vec2 cellSize = {1.0f,1.0f};
        glm::vec2 gapSize = {0.0f, 0.0f};

};