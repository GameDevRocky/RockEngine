#include "engine/components/Transform.hpp"


YAML::Node Transform::Serialize(){
        YAML::Node node;
        node["type"] = GetTypeName();
        node["position"]["x"] = position.x;
        node["position"]["y"] = position.y;
        node["rotation"] = rotation;
        node["scale"]["x"] = scale.x;
        node["scale"]["y"] = scale.y;
        return node;
    }

void Transform::Deserialize(const YAML::Node& node){
        Serializable::Deserialize(node);
        if (node["position"]) {
            position.x = node["position"][0].as<float>();
            position.y = node["position"][1].as<float>();
        }
        if (node["rotation"]) rotation = node["rotation"].as<float>();
        if (node["scale"]) {
            scale.x = node["scale"][0].as<float>();
            scale.y = node["scale"][1].as<float>();
        }
    }


 glm::mat4 Transform::GetTransformMatrix() const {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(position, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(scale, 1.0f));
        return transform;
    }