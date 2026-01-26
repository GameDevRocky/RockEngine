#pragma once

class Container;

class RuntimeObject {
public:
    virtual void OnEnterPlayMode() {}
    virtual void OnExitPlayMode() {}

    Container* GetContainer() const { return container; }
    virtual void Attach(Container* inContainer) { container = inContainer; }

protected:
    RuntimeObject() = default;
    virtual ~RuntimeObject() = default;

    Container* container = nullptr;
};