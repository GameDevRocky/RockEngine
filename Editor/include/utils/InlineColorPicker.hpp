#pragma once

#include <functional>
#include <QColor>
#include <QWidget>

class QFrame;
class QLineEdit;
class QSpinBox;
class QToolButton;

// Compact, inspector-owned color editor. The swatch is always visible; its
// disclosure control reveals the detailed editor in-place, without opening a
// top-level window or entering another event loop.
class InlineColorPicker : public QWidget {
public:
    explicit InlineColorPicker(QWidget* parent = nullptr);

    void SetColor(const QColor& color);
    QColor GetColor() const { return m_color; }

    // Two-column property hosts use this to place the expanded editor beneath
    // the label/swatch row at the full available width. Other hosts can leave it
    // embedded and retain the compact single-column behavior.
    QWidget* TakeExpandedWidget();

    std::function<void(const QColor&)> onColorChanged;

private:
    class SaturationValueField;
    class GradientSlider;
    class SwatchButton;

    void SetExpanded(bool expanded);
    void ApplyColor(const QColor& color, bool notify);
    void SyncControls();
    void ApplyHexText();

    QColor m_color{Qt::white};
    qreal m_hue = 0.0;
    bool m_syncing = false;

    QToolButton* m_toggle = nullptr;
    SwatchButton* m_swatch = nullptr;
    QFrame* m_body = nullptr;
    SaturationValueField* m_svField = nullptr;
    GradientSlider* m_hueSlider = nullptr;
    GradientSlider* m_alphaSlider = nullptr;
    QSpinBox* m_channels[4]{};
    QLineEdit* m_hexEdit = nullptr;
};
