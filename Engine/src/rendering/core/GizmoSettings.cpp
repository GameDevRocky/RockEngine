#include "engine/rendering/core/GizmoSettings.hpp"

GizmoSettings& GizmoSettings::Get() {
    static GizmoSettings instance;
    return instance;
}

void GizmoSettings::SetEnabled(bool v) {
    if (m_enabled == v) return;
    m_enabled = v;
    Notify(CHANGED_EVENT);
}

bool GizmoSettings::IsCategoryVisible(Category c) const {
    const int i = static_cast<int>(c);
    if (i < 0 || i >= kCount) return false;
    return m_categories[i];
}

void GizmoSettings::SetCategoryVisible(Category c, bool v) {
    const int i = static_cast<int>(c);
    if (i < 0 || i >= kCount) return;
    if (m_categories[i] == v) return;
    m_categories[i] = v;
    Notify(CHANGED_EVENT);
}

const char* GizmoSettings::CategoryLabel(Category c) {
    switch (c) {
        case Category::Cameras:           return "Cameras";
        case Category::Lights:            return "Lights";
        case Category::AudioSources:      return "Audio Sources";
        case Category::Joints:            return "Joints";
        case Category::ComponentIcons:    return "Component Icons";
        case Category::SelectionOutlines: return "Selection Outlines";
        case Category::Count:             break;
    }
    return "";
}
