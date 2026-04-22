#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include "engine/utils/EngineUtils.hpp"
#include "engine/core/Observable.hpp"
#include <memory>
#include "engine/utils/IVisitor.hpp"


class Registry;

class Serializable : public Observable {
public:
    virtual YAML::Node Serialize();

    virtual void Deserialize(const YAML::Node& node);

    virtual std::string GetTypeName(){return "Serializable";};

    void SetID(const std::string& id) { this->id = id; }
    const std::string& GetID() const { return id; }

    virtual Serializable* Copy() { return nullptr; }
    virtual void Accept(IVisitor* v){};

    Serializable();

    virtual ~Serializable() = default; 


protected:
    std::string id;
};
