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

    // const so that Component's `GetTypeName() const = 0` genuinely OVERRIDES this rather
    // than declaring a second, separate virtual that merely overloads it. While the two
    // differed in constness, calling GetTypeName() through a Serializable* -- which is
    // exactly what SerializableFactory::Create() hands back -- dispatched here and returned
    // "Serializable" for every component in the engine.
    virtual std::string GetTypeName() const { return "Serializable"; };

    void SetID(const std::string& id) { this->id = id; }
    const std::string& GetID() const { return id; }

    virtual Serializable* Copy() { return nullptr; }
    virtual void Accept(IVisitor* v){};

    Serializable();

    virtual ~Serializable() = default; 


protected:
    std::string id;
};
