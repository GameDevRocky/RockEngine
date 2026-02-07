#pragma once
#include "engine/serialization/Serializable.hpp"

class Container;

class RuntimeObject : public Serializable {
public:

    enum State {
        Allocated,
        Loaded,
        Initialized,
        PostInitialized,
        Awakened,
        Started,
    };

    virtual void Deserialize(const YAML::Node& node) override {state = State::Loaded;}
    virtual void Init(){state = State::Initialized;}
    virtual void PostInit(){state = State::PostInitialized;}
    virtual void Awake(){state = State::Awakened;}
    virtual void Start(){state = State::Started;}
    virtual void Update(){}
    virtual void Shutdown(){}

    Container* GetContainer() const { return container; }
    virtual void Attach(Container* inContainer) { container = inContainer; }
    virtual RuntimeObject* Copy(){ return nullptr; }
    virtual RuntimeObject* Copy(Container* container){ return nullptr; }

protected:
    RuntimeObject() = default;
    virtual ~RuntimeObject() = default;
    Container* container = nullptr;
    State state =  State::Allocated;

};