#include "engine/components/Grid.hpp"
#include "engine/components/Transform.hpp"


void Grid::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);
    this->cellSize.x = node["cellSize"][0].as<float>();
    this->cellSize.y = node["cellSize"][1].as<float>();
    state = State::Loaded;
}

YAML::Node Grid::Serialize(){
    YAML::Node node = Component::Serialize();
    node["cellSize"][0] = this->cellSize.x;
    node["cellSize"][1] = this->cellSize.y;
    return node;
}


Grid* Grid::Copy(){
    Grid* c = new Grid();
    c->id            = id;
    c->enabled       = enabled;
    c->gameobject_id = gameobject_id;
    c->cellSize = cellSize;
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
    Notify(SIZE_CHANGED_EVENT, gapSize);  
}

glm::vec2 Grid::CellToWorld(const glm::vec2& cell){
    float x;
    float y;
    const glm::vec2 worldPos = GetTransform()->GetWorldPosition();
    x = cell.x * (cellSize.x + gapSize.x) + worldPos.x;
    y = cell.y * (cellSize.y + gapSize.y) + worldPos.y;
    return {x, y};
}

glm::ivec2 Grid::WorldToCell(const glm::vec2& origin){
    const glm::vec2 worldPos = GetTransform()->GetWorldPosition();
    int x = std::floor((worldPos.x - origin.x) / (cellSize.x + gapSize.x));
    int y = std::floor((worldPos.y - origin.y) / (cellSize.y + gapSize.y));
    return {x, y};
}