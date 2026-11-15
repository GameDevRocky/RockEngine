#pragma once

#include <algorithm>
#include <cstdlib>
#include <QSlider>
#include <QWheelEvent>

namespace WheelScale {

// Apply the editor's Ctrl+wheel zoom gesture to an existing size slider. The
// slider remains the single source of truth, so its range clamps the result and
// its valueChanged handlers continue to own all layout updates.
inline bool AdjustSlider(QSlider* slider, QWheelEvent* event, int unitsPerStep = 8)
{
    if (!slider || !event ||
        !event->modifiers().testFlag(Qt::ControlModifier))
        return false;

    int delta = event->angleDelta().y();
    if (delta == 0) delta = event->pixelDelta().y();
    if (delta == 0) return false;

    const int steps = std::max(1, std::abs(delta) / 120);
    slider->setValue(slider->value() + (delta > 0 ? steps : -steps) * unitsPerStep);
    event->accept();
    return true;
}

} // namespace WheelScale
