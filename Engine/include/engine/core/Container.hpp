#pragma once
#include "engine/core/Observable.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/RuntimeObject.hpp"

#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

class Container : public RuntimeObject {
public:
	enum class Mode {
		Editor,
		Runtime,
		Paused
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

	void Init() override;
	void PostInit() override;
	void Awake() override;
	void Start() override;
	void Update();
	void Shutdown();

	std::vector<System*> GetAllSystems(){return systems;}

	template <typename T>
	void AddSystem(T* system) {
		static_assert(std::is_base_of_v<System, T>, "T must derive from System");
		if (!system) return;

		systems.push_back(system);
		systemIndex[typeid(*system)] = system;
		system->Attach(this);
	}

	template <typename T>
	T* FindSystem() {
		static_assert(std::is_base_of_v<System, T>, "T must derive from System");
		auto it = systemIndex.find(typeid(T));
		if (it != systemIndex.end())
			return static_cast<T*>(it->second);
		return nullptr;
	}
	
	Container* Copy();

private:
    bool initialized = false;
	Mode mode = Mode::Editor;
	std::vector<System*> systems;
	std::unordered_map<std::type_index, System*> systemIndex;

};