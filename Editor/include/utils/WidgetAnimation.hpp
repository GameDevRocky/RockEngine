#pragma once

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QWidget>

namespace WidgetAnimation {

inline void FadeIn(QWidget* widget, int durationMs = 360)
{
    if (!widget || widget->graphicsEffect()) return;

    auto* opacity = new QGraphicsOpacityEffect(widget);
    opacity->setOpacity(0.0);
    widget->setGraphicsEffect(opacity);

    auto* animation = new QPropertyAnimation(opacity, "opacity", widget);
    animation->setDuration(durationMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget, opacity, animation]() {
                         animation->deleteLater();
                         if (widget->graphicsEffect() == opacity)
                             widget->setGraphicsEffect(nullptr);
                     });
    animation->start();
}

} // namespace WidgetAnimation
