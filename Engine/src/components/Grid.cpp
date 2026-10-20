#include "engine/components/Grid.hpp"



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
