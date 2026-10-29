#include "engine/components/Grid.hpp"
#include "engine/components/Transform.hpp"

#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>


void Grid::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);
    this->cellSize.x = node["cellSize"][0].as<float>();
    this->cellSize.y = node["cellSize"][1].as<float>();
    if (node["gapSize"]){
        this->gapSize.x = node["gapSize"][0].as<float>();
        this->gapSize.y = node["gapSize"][1].as<float>();
    }
    state = State::Loaded;
}

YAML::Node Grid::Serialize(){
    YAML::Node node = Component::Serialize();
    node["cellSize"][0] = this->cellSize.x;
    node["cellSize"][1] = this->cellSize.y;
    node["gapSize"][0] = this->gapSize.x;
    node["gapSize"][1] = this->gapSize.y;
    return node;
}


Grid* Grid::Copy(){
    Grid* c = new Grid();
    c->id            = id;
    c->enabled       = enabled;
    c->gameobject_id = gameobject_id;
    c->cellSize = cellSize;
    c->gapSize = gapSize;
    return c;
}

void Grid::Accept(IVisitor* v){
    v->Visit(this);
}

void Grid::SetSize(glm::vec2 size){
    if (this->cellSize == size) return;
    this->cellSize = size;
    Notify(SIZE_CHANGED_EVENT, size);  
}

void Grid::SetGap(glm::vec2 gap){
    if (this->gapSize == gap) return;
    this->gapSize = gap;
    Notify(GAP_CHANGED_EVENT, gapSize);
}

glm::vec2 Grid::CellToWorld(const glm::ivec2& cell){
    const glm::vec2 stride = cellSize + gapSize;
    const glm::vec2 local = glm::vec2(cell) * stride;

    Transform* transform = GetTransform();
    if (transform == nullptr) return local;

    const glm::vec4 world = transform->GetWorldMatrix()
                          * glm::vec4(local, 0.0f, 1.0f);
    return {world.x, world.y};
}

glm::ivec2 Grid::WorldToCell(const glm::vec2& world){
    glm::vec2 local = world;
    if (Transform* transform = GetTransform()) {
        const glm::mat4 worldMatrix = transform->GetWorldMatrix();
        if (std::abs(glm::determinant(worldMatrix)) <= 1.0e-6f) return {0, 0};

        const glm::vec4 localPoint = glm::inverse(worldMatrix)
                                   * glm::vec4(world, 0.0f, 1.0f);
        local = {localPoint.x, localPoint.y};
    }

    const glm::vec2 stride = cellSize + gapSize;
    if (std::abs(stride.x) <= 1.0e-6f || std::abs(stride.y) <= 1.0e-6f)
        return {0, 0};

    return {
        static_cast<int>(std::floor(local.x / stride.x)),
        static_cast<int>(std::floor(local.y / stride.y))
    };
}
