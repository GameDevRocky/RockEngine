#pragma once
#include "engine/serialization/Serializable.hpp"

class Resource : public Serializable{

public:
    static inline const Event NAME_CHANGED_EVENT = Resource::CreateEvent();

    virtual void Init(){}
    virtual void Awake(){}
    virtual void Update(){}
    virtual void Shutdown(){}
    virtual YAML::Node Serialize() override { return {}; }
    virtual void Deserialize(const YAML::Node& node) override;
    virtual void Validate();

    void SetName(std::string name);
    std::string GetName() {return name;}

protected:
    std::string name;
};