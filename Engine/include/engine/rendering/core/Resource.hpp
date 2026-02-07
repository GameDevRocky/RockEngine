#pragma once
#include "engine/serialization/Serializable.hpp"

class Resource : public Serializable{

public:
    virtual void Init(){}
    virtual void Awake(){}
    virtual void Update(){}
    virtual void Shutdown(){}

private:



};