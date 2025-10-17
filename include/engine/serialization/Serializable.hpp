#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include "engine/utils/IDGenerator.hpp"
#include "engine/core/Observable.hpp"
#include <memory>

class Registry;

class Serializable : public Observable {
public:
    Serializable();
    virtual ~Serializable();
    
    virtual YAML::Node Serialize();

    virtual void Deserialize(const YAML::Node& node);
    virtual std::string GetTypeName() const = 0;
    const std::string& GetID() const { return id; }

protected:
    std::string id;
};
