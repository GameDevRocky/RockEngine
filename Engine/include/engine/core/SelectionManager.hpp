#pragma once
#include "engine/core/System.hpp"
#include "engine/serialization/Serializable.hpp"
#include <string>

class GameObject;

class SelectionManager : public System {
public:
    static inline const Event SELECTION_CHANGED_EVENT = Observable::CreateEvent();

    SelectionManager() = default;
    ~SelectionManager() override = default;

    void Init() override;
    void Shutdown() override;

    SelectionManager* Copy() override;
    SelectionManager* Copy(Container* container) override;

    void Select(const std::string& objectId);
    void Deselect();

    Serializable* GetSerializable() const;

    const std::string& GetSelectedId() const { return selectedObjectId; }
    bool HasSelection() const { return !selectedObjectId.empty(); }

private:
    Registry* registry = nullptr;
    std::string selectedObjectId;
    int m_shutdownSubId = -1;
};
