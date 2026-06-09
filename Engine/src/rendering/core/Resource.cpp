#include "engine/rendering/core/Resource.hpp"



void Resource::Validate(){}

void Resource::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
    name = node["name"].as<std::string>();

}

void Resource::SetName(std::string name){
    this->name = name;
    Notify(NAME_CHANGED_EVENT);
}

void Resource::SetFilePath(const std::string& path){
    m_filePath = path;
    Notify(FILE_PATH_CHANGED_EVENT);
}