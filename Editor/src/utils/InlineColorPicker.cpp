#include "utils/InlineColorPicker.hpp"

#include <algorithm>
#include <array>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kCheckerSize = 5;

void PaintCheckerboard(QPainter& painter, const QRect& rect) {
    painter.fillRect(rect, QColor(68, 68, 70));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(105, 105, 108));
    for (int y = rect.top(); y <= rect.bottom(); y += kCheckerSize) {
        for (int x = rect.left(); x <= rect.right(); x += kCheckerSize) {
            const int column = (x - rect.left()) / kCheckerSize;
            const int row = (y - rect.top()) / kCheckerSize;
            if ((column + row) % 2 == 0)
                painter.drawRect(QRect(x, y, kCheckerSize, kCheckerSize).intersected(rect));
        }
    }
}

QString ColorToHex(const QColor& color) {
    return QString("#%1%2%3%4")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'))
        .arg(color.alpha(), 2, 16, QLatin1Char('0'))
        .toUpper();
}
}

class InlineColorPicker::SwatchButton : public QWidget {
public:
    explicit SwatchButton(QWidget* parent = nullptr) : QWidget(parent) {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip("Expand color picker");
    }

    void SetColor(const QColor& color) { m_color = color; update(); }
    std::function<void()> onClicked;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(outer, 6.0, 6.0);
        painter.save();
        painter.setClipPath(clip);
        PaintCheckerboard(painter, outer.toAlignedRect());
        painter.fillRect(outer, isEnabled() ? m_color : QColor(75, 75, 75, 170));
        if (underMouse() && isEnabled())
            painter.fillRect(outer, QColor(255, 255, 255, 18));
        painter.restore();

        painter.setPen(QPen(underMouse() && isEnabled()
            ? QColor(131, 176, 145) : QColor(51, 51, 51), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outer, 6.0, 6.0);
    }

    void enterEvent(QEnterEvent* event) override { update(); QWidget::enterEvent(event); }
    void leaveEvent(QEvent* event) override { update(); QWidget::leaveEvent(event); }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && isEnabled()) {
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && isEnabled()) {
            if (onClicked) onClicked();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    QColor m_color{Qt::white};
};

class InlineColorPicker::SaturationValueField : public QWidget {
public:
    explicit SaturationValueField(QWidget* parent = nullptr) : QWidget(parent) {
        setCursor(Qt::CrossCursor);
        setMinimumSize(90, 105);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void SetHue(qreal hue) { m_hue = std::clamp(hue, 0.0, 1.0); update(); }
    void SetSaturationValue(qreal saturation, qreal value) {
        m_saturation = std::clamp(saturation, 0.0, 1.0);
        m_value = std::clamp(value, 0.0, 1.0);
        update();
    }

    std::function<void(qreal, qreal)> onChanged;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF field = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        QPainterPath clip;
        clip.addRoundedRect(field, 5.0, 5.0);
        painter.save();
        painter.setClipPath(clip);
        painter.fillRect(field, QColor::fromHsvF(m_hue, 1.0, 1.0));

        QLinearGradient saturation(field.topLeft(), field.topRight());
        saturation.setColorAt(0.0, Qt::white);
        saturation.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(field, saturation);

        QLinearGradient value(field.topLeft(), field.bottomLeft());
        value.setColorAt(0.0, QColor(0, 0, 0, 0));
        value.setColorAt(1.0, Qt::black);
        painter.fillRect(field, value);
        painter.restore();

        painter.setPen(QPen(QColor(36, 36, 36), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(field, 5.0, 5.0);

        const QPointF marker(field.left() + m_saturation * field.width(),
                             field.top() + (1.0 - m_value) * field.height());
        painter.setPen(QPen(Qt::black, 3.0));
        painter.drawEllipse(marker, 5.0, 5.0);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawEllipse(marker, 4.0, 4.0);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            UpdateFromPosition(event->position());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            UpdateFromPosition(event->position());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    void UpdateFromPosition(const QPointF& position) {
        const qreal usableWidth = std::max(1, width() - 1);
        const qreal usableHeight = std::max(1, height() - 1);
        m_saturation = std::clamp(position.x() / usableWidth, 0.0, 1.0);
        m_value = 1.0 - std::clamp(position.y() / usableHeight, 0.0, 1.0);
        update();
        if (onChanged) onChanged(m_saturation, m_value);
    }

    qreal m_hue = 0.0;
    qreal m_saturation = 0.0;
    qreal m_value = 1.0;
};

class InlineColorPicker::GradientSlider : public QWidget {
public:
    enum class Kind { Hue, Alpha };

    explicit GradientSlider(Kind kind, QWidget* parent = nullptr)
        : QWidget(parent), m_kind(kind)
    {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(16);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void SetValue(qreal value) { m_value = std::clamp(value, 0.0, 1.0); update(); }
    void SetColor(const QColor& color) { m_color = color; update(); }
    std::function<void(qreal)> onChanged;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track = QRectF(rect()).adjusted(0.5, 2.5, -0.5, -2.5);
        QPainterPath clip;
        clip.addRoundedRect(track, 4.0, 4.0);

        painter.save();
        painter.setClipPath(clip);
        if (m_kind == Kind::Alpha) {
            PaintCheckerboard(painter, track.toAlignedRect());
            QLinearGradient gradient(track.topLeft(), track.topRight());
            QColor transparent = m_color;
            transparent.setAlpha(0);
            QColor opaque = m_color;
            opaque.setAlpha(255);
            gradient.setColorAt(0.0, transparent);
            gradient.setColorAt(1.0, opaque);
            painter.fillRect(track, gradient);
        } else {
            QLinearGradient gradient(track.topLeft(), track.topRight());
            for (int i = 0; i <= 6; ++i)
                gradient.setColorAt(i / 6.0, QColor::fromHsvF(i / 6.0, 1.0, 1.0));
            painter.fillRect(track, gradient);
        }
        painter.restore();

        painter.setPen(QPen(QColor(36, 36, 36), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track, 4.0, 4.0);

        const qreal x = track.left() + m_value * track.width();
        painter.setPen(QPen(Qt::black, 3.0));
        painter.drawLine(QPointF(x, track.top() - 1.0), QPointF(x, track.bottom() + 1.0));
        painter.setPen(QPen(Qt::white, 1.0));
        painter.drawLine(QPointF(x, track.top()), QPointF(x, track.bottom()));
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            UpdateFromPosition(event->position().x());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            UpdateFromPosition(event->position().x());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    void UpdateFromPosition(qreal x) {
        m_value = std::clamp(x / std::max(1, width() - 1), 0.0, 1.0);
        update();
        if (onChanged) onChanged(m_value);
    }

    Kind m_kind;
    qreal m_value = 0.0;
    QColor m_color{Qt::white};
};

InlineColorPicker::InlineColorPicker(QWidget* parent) : QWidget(parent) {
    setObjectName("InlineColorPicker");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(3);

    m_toggle = new QToolButton(header);
    m_toggle->setObjectName("ColorPickerToggle");
    m_toggle->setArrowType(Qt::RightArrow);
    m_toggle->setCheckable(true);
    m_toggle->setFixedSize(18, 24);
    m_toggle->setToolTip("Expand color picker");
    headerLayout->addWidget(m_toggle);

    m_swatch = new SwatchButton(header);
    headerLayout->addWidget(m_swatch, 1);
    outer->addWidget(header);

    m_body = new QFrame(this);
    m_body->setObjectName("InlineColorPickerBody");
    auto* bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(7, 7, 7, 7);
    bodyLayout->setSpacing(5);

    m_svField = new SaturationValueField(m_body);
    bodyLayout->addWidget(m_svField);

    auto addGradientRow = [bodyLayout, this](const QString& name, GradientSlider* slider) {
        auto* row = new QWidget(m_body);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto* label = new QLabel(name, row);
        label->setObjectName("ColorPickerChannelLabel");
        label->setFixedWidth(12);
        layout->addWidget(label);
        layout->addWidget(slider, 1);
        bodyLayout->addWidget(row);
    };

    m_hueSlider = new GradientSlider(GradientSlider::Kind::Hue, m_body);
    addGradientRow("H", m_hueSlider);
    m_alphaSlider = new GradientSlider(GradientSlider::Kind::Alpha, m_body);
    addGradientRow("A", m_alphaSlider);

    auto* channels = new QWidget(m_body);
    auto* channelLayout = new QGridLayout(channels);
    channelLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->setHorizontalSpacing(4);
    channelLayout->setVerticalSpacing(3);
    const std::array<const char*, 4> names{"R", "G", "B", "A"};
    for (int i = 0; i < 4; ++i) {
        auto* label = new QLabel(names[i], channels);
        label->setObjectName("ColorPickerChannelLabel");
        m_channels[i] = new QSpinBox(channels);
        m_channels[i]->setObjectName("ColorChannelSpin");
        m_channels[i]->setRange(0, 255);
        m_channels[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_channels[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        const int row = i / 2;
        const int column = (i % 2) * 2;
        channelLayout->addWidget(label, row, column);
        channelLayout->addWidget(m_channels[i], row, column + 1);
        channelLayout->setColumnStretch(column + 1, 1);
        QObject::connect(m_channels[i], &QSpinBox::valueChanged, this, [this](int) {
            if (m_syncing) return;
            ApplyColor(QColor(m_channels[0]->value(), m_channels[1]->value(),
                              m_channels[2]->value(), m_channels[3]->value()), true);
        });
    }
    bodyLayout->addWidget(channels);

    auto* hexRow = new QWidget(m_body);
    auto* hexLayout = new QHBoxLayout(hexRow);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    hexLayout->setSpacing(5);
    auto* hexLabel = new QLabel("Hex", hexRow);
    hexLabel->setObjectName("ColorPickerHexLabel");
    hexLayout->addWidget(hexLabel);
    m_hexEdit = new QLineEdit(hexRow);
    m_hexEdit->setObjectName("ColorHexEdit");
    m_hexEdit->setMaxLength(9);
    m_hexEdit->setPlaceholderText("#RRGGBBAA");
    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("#[0-9A-Fa-f]{0,8}"), m_hexEdit));
    hexLayout->addWidget(m_hexEdit, 1);
    bodyLayout->addWidget(hexRow);

    outer->addWidget(m_body);
    m_body->hide();

    QObject::connect(m_toggle, &QToolButton::toggled, this,
                     [this](bool expanded) { SetExpanded(expanded); });
    m_swatch->onClicked = [this]() { m_toggle->setChecked(!m_toggle->isChecked()); };

    m_svField->onChanged = [this](qreal saturation, qreal value) {
        ApplyColor(QColor::fromHsvF(m_hue, saturation, value, m_color.alphaF()), true);
    };
    m_hueSlider->onChanged = [this](qreal hue) {
        m_hue = hue;
        float ignoredHue = 0.0f;
        float saturation = 0.0f;
        float value = 0.0f;
        m_color.getHsvF(&ignoredHue, &saturation, &value);
        ApplyColor(QColor::fromHsvF(m_hue, saturation, value, m_color.alphaF()), true);
    };
    m_alphaSlider->onChanged = [this](qreal alpha) {
        QColor color = m_color;
        color.setAlphaF(alpha);
        ApplyColor(color, true);
    };
    QObject::connect(m_hexEdit, &QLineEdit::editingFinished,
                     this, [this]() { ApplyHexText(); });

    SyncControls();
}

void InlineColorPicker::SetColor(const QColor& color) {
    ApplyColor(color, false);
}

QWidget* InlineColorPicker::TakeExpandedWidget() {
    if (m_body->parentWidget() == this) {
        layout()->removeWidget(m_body);
        m_body->setParent(nullptr);
    }
    return m_body;
}

void InlineColorPicker::SetExpanded(bool expanded) {
    m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_toggle->setToolTip(expanded ? "Collapse color picker" : "Expand color picker");
    m_swatch->setToolTip(expanded ? "Collapse color picker" : "Expand color picker");
    m_body->setVisible(expanded);
    m_body->updateGeometry();
    updateGeometry();
}

void InlineColorPicker::ApplyColor(const QColor& color, bool notify) {
    if (!color.isValid()) return;

    m_color = color.toRgb();
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    m_color.getHsvF(&hue, &saturation, &value);
    if (hue >= 0.0)
        m_hue = hue;
    SyncControls();

    if (notify && onColorChanged)
        onColorChanged(m_color);
}

void InlineColorPicker::SyncControls() {
    m_syncing = true;

    float ignoredHue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    m_color.getHsvF(&ignoredHue, &saturation, &value);
    m_swatch->SetColor(m_color);
    m_svField->SetHue(m_hue);
    m_svField->SetSaturationValue(saturation, value);
    m_hueSlider->SetValue(m_hue);
    m_alphaSlider->SetColor(QColor(m_color.red(), m_color.green(), m_color.blue()));
    m_alphaSlider->SetValue(m_color.alphaF());

    const std::array<int, 4> values{
        m_color.red(), m_color.green(), m_color.blue(), m_color.alpha()};
    for (int i = 0; i < 4; ++i) {
        const QSignalBlocker blocker(m_channels[i]);
        m_channels[i]->setValue(values[i]);
    }
    {
        const QSignalBlocker blocker(m_hexEdit);
        m_hexEdit->setText(ColorToHex(m_color));
    }

    m_syncing = false;
}

void InlineColorPicker::ApplyHexText() {
    QString text = m_hexEdit->text().trimmed();
    if (text.startsWith('#')) text.removeFirst();
    if (text.size() != 6 && text.size() != 8) {
        SyncControls();
        return;
    }

    bool ok = false;
    const uint rgba = text.toUInt(&ok, 16);
    if (!ok) {
        SyncControls();
        return;
    }

    QColor parsed;
    if (text.size() == 6) {
        parsed = QColor((rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff, m_color.alpha());
    } else {
        parsed = QColor((rgba >> 24) & 0xff, (rgba >> 16) & 0xff,
                        (rgba >> 8) & 0xff, rgba & 0xff);
    }
    ApplyColor(parsed, true);
}
