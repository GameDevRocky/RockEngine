#pragma once
#include "engine/core/Observable.hpp"
#include "engine/core/RuntimeObject.hpp"

class Container;

class System : public Observable, public RuntimeObject{
public:
    virtual ~System() = default;

    virtual void Attach(Container* container){
        this->container = container;
    }   

    virtual void Init() {}
    virtual void PostInit(){}
    virtual void Update() {}
    virtual void Shutdown() {}
    virtual System* Copy(){ return nullptr; };
    System* Copy(Container* container) override {
        System* copy = this->Copy();
        if (copy)
            copy->Attach(container);
        return copy;
    }

protected:
    Container* container = nullptr;
    System() = default; 
};
