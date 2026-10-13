#include "engine/input/CursorManager.hpp"

#include "engine/rendering/core/AssetManager.hpp"

#include <algorithm>

void CursorManager::Changed() {
    ++revision;
    Notify(CHANGED_EVENT);
}

void CursorManager::SetVisible(bool value) {
    if (visible == value) return;
    visible = value;
    Changed();
}

void CursorManager::SetLockState(CursorLockState value) {
    if (lockState == value) return;
    lockState = value;
    Changed();
}

bool CursorManager::SetSprite(const std::string& id, glm::vec2 pixelHotspot) {
    if (!id.empty() && !AssetManager::Get().GetSprite(id))
        return false;

    pixelHotspot.x = std::max(pixelHotspot.x, 0.0f);
    pixelHotspot.y = std::max(pixelHotspot.y, 0.0f);
    if (spriteId == id && hotspot == pixelHotspot) return true;

    spriteId = id;
    hotspot = pixelHotspot;
    Changed();
    return true;
}

void CursorManager::ResetSprite() {
    SetSprite({}, {0.0f, 0.0f});
}

void CursorManager::Reset() {
    if (visible && lockState == CursorLockState::None && spriteId.empty() &&
        hotspot == glm::vec2(0.0f))
        return;

    visible = true;
    lockState = CursorLockState::None;
    spriteId.clear();
    hotspot = {0.0f, 0.0f};
    Changed();
}
