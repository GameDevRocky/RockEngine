#pragma once
#include "engine/core/Observable.hpp"

class Container;

class System : public Observable{
public:
    virtual ~System() = default;

    virtual void Attach(Container* container){
        this->container = container;
        OnAttach();
    }   
    virtual void OnAttach(){};
    virtual void OnEnterPlayMode(){};
    virtual void Init() {}
    virtual void PostInit(){}
    virtual void Update() {}
    virtual void Shutdown() {}
    virtual System* Copy(){ return nullptr; };

protected:
    Container* container = nullptr;
    System() = default; 
};
