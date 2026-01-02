#pragma once
#include <string>
#include "engine/utils/EngineUtils.hpp"
#include <memory>
#include "IObservable.hpp"
#include <yaml-cpp/yaml.h>

class Registry;

class ISerializable : public IObservable {
public:
    virtual ~ISerializable() = 0; 
    virtual YAML::Node Serialize() = 0;
    virtual void Deserialize(const YAML::Node& node) = 0;
    virtual void PostDeserialize(){} 
    virtual std::string GetTypeName() = 0;
    virtual const std::string& GetID() = 0;
};
