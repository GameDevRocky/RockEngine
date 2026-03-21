#include "utils/SceneTree.hpp"
#include "engine/core/Scene.hpp"


void SceneTree::Init(Scene* scene){
    this->scene_id = scene->GetID();

}

void SceneTree::Update(){


}

SceneTree::SceneTree(QWidget* parent): QTreeView(parent) {}