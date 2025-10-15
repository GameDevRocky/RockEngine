#pragma once

class System {
public:
    virtual ~System() = default;

    virtual void Init() {}
    virtual void Update() {}
    virtual void Shutdown() {}

protected:
    System() = default; // protected so derived classes can call it
};
