#pragma once
#include "engine/core/Observable.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"

class Container : public Observable {
public:
	enum class Mode {
		Editor,
		Runtime,
	};

	explicit Container(Mode mode = Mode::Editor);
	virtual ~Container() = default;

	Mode GetMode() const { return mode; }

	void SetMode(Mode newMode) {
		if (mode == newMode)
			return;
		mode = newMode;
		Notify();
	}

	void Init();
    void PostInit();
	void Update();
	void Shutdown();

    SceneManager* GetSceneManager(){return sceneManager;}
    TimeManager* GetTimeManager(){return timeManager;}
    InputManager* GetInputManager(){return inputManager;}
    Registry* GetRegistry(){return registry;}

	Container* Copy();

private:
    bool initialized = false;
	Mode mode;
    Registry* registry = nullptr;
    SceneManager* sceneManager = nullptr;
    TimeManager* timeManager = nullptr;
    InputManager* inputManager = nullptr;
};