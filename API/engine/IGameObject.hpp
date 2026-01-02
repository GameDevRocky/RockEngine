#pragma once
#include <string>
#include "ISerializable.hpp"

class IComponent;
class IScene;
class Transform;

class IGameObject : public ISerializable {
public:
    virtual ~IGameObject() {}
    virtual void AddComponent(const std::string& comp_typeName) = 0;

    template<typename T>
    T* GetComponent() {
        return static_cast<T*>(Internal_GetComponent(EngineUtils::TypeName<T>()));
    }

    virtual Transform* GetTransform() = 0;
    virtual std::string GetName() = 0;
    virtual IScene* GetScene() = 0;

    virtual void Update() = 0;
    virtual void FixedUpdate() = 0;
    virtual void LateUpdate() = 0;

protected:
    virtual IComponent* Internal_GetComponent(const std::string& typeName) = 0;

};