#pragma once
#include "engine/core/Observable.hpp"
#include "engine/core/RuntimeObject.hpp"

class Container;

class System : public RuntimeObject{
public:

    virtual System* Copy(){ return nullptr; }
    virtual System* Copy(Container* container){ return nullptr; }
    
    System() = default; 
    virtual ~System() = default;

protected:
};
