#pragma once
#include "engine/core/Observable.hpp"

class System : public Observable{
public:
    virtual ~System() = default;

    virtual void Init() {}
    virtual void Update() {}
    virtual void Shutdown() {}

protected:
    System() = default; 
};
