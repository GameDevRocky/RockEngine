#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Observable.hpp"
#include <memory>

class Registry;

class Serializable : public Observable {
public:
    virtual YAML::Node Serialize();

    virtual void Deserialize(const YAML::Node& node);

    virtual std::string GetTypeName(){return "Serializable";};

    const std::string& GetID() const { return id; }

    virtual Serializable* Copy() { return nullptr; }

    Serializable() = default;

    virtual ~Serializable() = default; 

protected:
    std::string id;
};
