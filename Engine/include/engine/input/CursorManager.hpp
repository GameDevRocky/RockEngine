#pragma once

#include "engine/core/Observable.hpp"

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

// Window-system-neutral cursor intent. Cursor hardware belongs to the process, not a
// Container, so this state must not be copied when play mode deep-copies the world. The Qt
// editor host and SDL player host observe it and apply the corresponding native operations.
enum class CursorLockState : int {
    None = 0,
    Confined,
    Locked
};

class CursorManager : public Observable {
public:
    static inline const Event CHANGED_EVENT = Observable::CreateEvent();

    static CursorManager& Get() {
        static CursorManager instance;
        return instance;
    }

    bool IsVisible() const { return visible; }
    CursorLockState GetLockState() const { return lockState; }
    const std::string& GetSpriteID() const { return spriteId; }
    glm::vec2 GetHotspot() const { return hotspot; }
    std::uint64_t GetRevision() const { return revision; }

    void SetVisible(bool value);
    void SetLockState(CursorLockState value);

    // An empty id restores the platform cursor. Returns false for an unknown non-empty id.
    bool SetSprite(const std::string& id, glm::vec2 pixelHotspot = {0.0f, 0.0f});
    void ResetSprite();

    // Restore the complete safe/default state. Used when play mode ends so script-owned
    // capture and cursor art cannot leak into the editor.
    void Reset();

private:
    CursorManager() = default;
    void Changed();

    bool visible = true;
    CursorLockState lockState = CursorLockState::None;
    std::string spriteId;
    glm::vec2 hotspot{0.0f, 0.0f};
    std::uint64_t revision = 0;
};
