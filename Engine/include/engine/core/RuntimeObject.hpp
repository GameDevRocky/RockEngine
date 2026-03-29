#pragma once
#include "engine/serialization/Serializable.hpp"

class Container;
class Registry;
class PhysicsSystem;
class SceneManager;
class TimeManager;
class InputManager;

class RuntimeObject : public Serializable {
public:

    static inline const Event INIT_EVENT = Observable::CreateEvent();
    static inline const Event POST_INIT_EVENT = Observable::CreateEvent();
    static inline const Event AWAKE_EVENT = Observable::CreateEvent();
    static inline const Event START_EVENT = Observable::CreateEvent();
    static inline const Event SHUTDOWN_EVENT = Observable::CreateEvent();
    static inline const Event CHANGED_EVENT = Observable::CreateEvent();

    RuntimeObject() = default;
    virtual ~RuntimeObject() = default;
    
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
    virtual void Attach(Container* container);
    virtual RuntimeObject* Copy(){ return nullptr; }
    virtual RuntimeObject* Copy(Container* container){ return nullptr; }

protected:
    Container* container = nullptr;
    Registry* registry = nullptr;
    TimeManager* timeManager = nullptr;
    PhysicsSystem* physicsSystem = nullptr;
    InputManager* inputManager = nullptr;
    SceneManager* sceneManager = nullptr;
    State state =  State::Allocated;

};