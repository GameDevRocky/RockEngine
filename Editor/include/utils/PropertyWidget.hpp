#pragma once
#include <functional>
#include <algorithm>
#include <QWidget>
#include <QPointer>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QPainter>
#include <QResizeEvent>
#include <QComboBox>
#include <QSlider>
#include <QStyle>
#include <QApplication>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>
#include <QEvent>
#include <QIcon>
#include <QSize>
#include <glm/glm.hpp>
#include "engine/utils/Properties.hpp"
#include "utils/AssetPickerWidget.hpp"
#include "utils/AssetThumbnails.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/EditorUtils.hpp"
#include "utils/RefDropFilter.hpp"
#include "dock-widgets/FolderViewGui.hpp"
#include <QFileInfo>


class PropertyWidgetBase {
public:
    virtual ~PropertyWidgetBase() = default;
    virtual QWidget* GetWidget() = 0;
    virtual bool IsValid() = 0;
};

template<typename T>
class PropertyWidget : public PropertyWidgetBase {
public:
    std::function<void(T)> onChanged;

    virtual void SetValue(const T& val) = 0;
    virtual T    GetValue() = 0;
};

// True when a descriptor asks for a display-only field, by either of the two
// routes that mean it. PropDesc::tag holds a single value, so Tags::READONLY and
// a type tag such as Tags::FLOAT are mutually exclusive -- a caller that needs
// both a type and read-only sets the PropDesc::ReadOnly() flag alongside its type
// tag instead. Script fields declared Reflect[T, ReadOnly()] arrive by that route.
//
// Widgets below apply this two different ways, deliberately. A control with a real
// read-only mode (QLineEdit, QAbstractSpinBox) uses it: edits are blocked while the
// value stays at full contrast and stays selectable/copyable, which is what
// read-only rows in the Camera and AudioClip inspectors have always looked like.
// A control with no such mode (checkbox, combo box, colour and picker buttons) is
// disabled instead, which the stylesheet already dresses properly -- see
// .QCheckBox::indicator:checked:disabled, which keeps a disabled `true` visibly
// checked. Don't unify these by disabling the spin boxes too: that would grey out
// every existing read-only row in the editor.
inline bool IsReadOnly(const Properties::PropDesc& desc) {
    return desc.readOnly || desc.tag == Properties::Tags::READONLY;
}

// True when a descriptor declares real bounds, i.e. Range() was called with
// something narrower than the whole float line. PropDesc::min/max default to
// -/+FLT_MAX rather than NaN, so std::isfinite would answer "yes" for a field
// that was never given a range -- this compares against those sentinels instead.
//
// This is the gate on both slider widgets. A slider is a position along a span,
// so without a span there is nothing to draw and nothing a drag could mean; the
// factory falls back to the ordinary numeric editor rather than rendering a
// control whose handle would never visibly move.
inline bool HasFiniteRange(const Properties::PropDesc& desc) {
    return desc.min > -std::numeric_limits<float>::max()
        && desc.max <  std::numeric_limits<float>::max()
        && desc.max >  desc.min;
}

// Shared setup for every numeric field in the inspector -- a lone float and each
// component of a vector -- so how they look is decided in one place rather than
// in four constructors that had already drifted apart.
//
// No up/down arrows. At inspector row height they are a ~7px target that is
// easier to miss than to hit, and they take that width from the number itself,
// which is the part worth reading. Values are typed.
//
// Note this also removes the only thing that used to distinguish a read-only
// field: FloatPropertyWidget dropped the buttons for READONLY and kept them
// otherwise. Read-only is now carried by setReadOnly() alone, which blocks
// editing but is not visually obvious -- worth styling if it matters.
//
// Size policy is deliberately NOT set here: a lone float is Ignored horizontally
// so the grid column drives its width, while a vector's components are Expanding
// so they share the row evenly. Callers keep that choice.
inline void ConfigureSpinBox(QDoubleSpinBox* spin, const Properties::PropDesc& desc) {
    spin->setRange(desc.min, desc.max);
    spin->setSingleStep(desc.step);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    // Read-only lives here rather than in each constructor so it reaches the
    // vector widgets too -- they build their components through this helper and
    // previously ignored the flag entirely. QAbstractSpinBox::stepEnabled()
    // returns StepNone when read-only, so the wheel and arrow keys are covered
    // as well as typing.
    if (IsReadOnly(desc)) spin->setReadOnly(true);
}

class FloatPropertyWidget : public PropertyWidget<float> {
public:
    explicit FloatPropertyWidget(const Properties::PropDesc& desc) {
        spin = new QDoubleSpinBox();
        ConfigureSpinBox(spin, desc);
        spin->setDecimals(desc.tag == Properties::Tags::INT ? 0 : 2);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        QObject::connect(spin, &QDoubleSpinBox::valueChanged, [this](double val) {
            if (onChanged) onChanged(static_cast<float>(val));
        });
    }

    QWidget* GetWidget() override { return spin; }
    bool IsValid() override { return !spin.isNull(); }

    void SetValue(const float& val) override {
        if (spin.isNull()) return;
        spin->blockSignals(true);
        spin->setValue(val);
        spin->blockSignals(false);
    }

    float GetValue() override {
        return spin.isNull() ? 0.0f : static_cast<float>(spin->value());
    }

private:
    QPointer<QDoubleSpinBox> spin;
};


// Decimals to show for a field whose increment is `step`. Mirrors what the plain
// float row does (2 places) while letting a coarse slider stop pretending it has
// hundredths -- Step(1) on a 0..100 range should read "42", not "42.00".
inline int DecimalsForStep(float step) {
    if (step >= 1.0f)  return 0;
    if (step >= 0.1f)  return 1;
    return 2;
}


// A bounded float: drag handle on the left, exact number on the right. The number
// is a real spin box, not a label, because a slider alone cannot express "exactly
// 0.75" and every other numeric row in the inspector is typeable.
//
// Reachable only through Tags::SLIDER *and* a bounded Range -- see HasFiniteRange
// and the factory below.
class SliderPropertyWidget : public PropertyWidget<float> {
public:
    explicit SliderPropertyWidget(const Properties::PropDesc& desc)
        : m_min(desc.min), m_max(desc.max),
          m_step(desc.step > 0.0f ? desc.step : 0.01f)
    {
        // QSlider is integer-only, so the span is quantised into step-sized ticks:
        // the handle then lands exactly on the increments the field declared.
        // Capped because a tiny Step over a wide Range would otherwise ask for
        // millions of positions, where a single pixel of drag skips hundreds of
        // values and the handle can never address most of them anyway.
        m_ticks = std::clamp(
            static_cast<int>(std::lround((m_max - m_min) / m_step)), 1, 10000);

        m_container = new QWidget();
        m_container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        m_slider = new QSlider(Qt::Horizontal);
        m_slider->setRange(0, m_ticks);
        m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(m_slider, 1);

        m_spin = new QDoubleSpinBox();
        ConfigureSpinBox(m_spin, desc);
        m_spin->setDecimals(DecimalsForStep(m_step));
        m_spin->setFixedWidth(54);
        m_spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        layout->addWidget(m_spin);

        // ConfigureSpinBox already made the number read-only; the slider has no
        // read-only mode of its own, so it is disabled (see IsReadOnly).
        if (IsReadOnly(desc)) m_slider->setEnabled(false);

        // Each direction writes the other with signals blocked. Without that the
        // echo comes straight back -- slider moves spin moves slider -- and the
        // rounding through the tick grid makes it oscillate rather than settle.
        QObject::connect(m_slider, &QSlider::valueChanged, [this](int tick) {
            if (m_syncing || m_spin.isNull()) return;
            m_syncing = true;
            const float v = FromTick(tick);
            m_spin->blockSignals(true);
            m_spin->setValue(v);
            m_spin->blockSignals(false);
            m_syncing = false;
            if (onChanged) onChanged(static_cast<float>(m_spin->value()));
        });
        QObject::connect(m_spin, &QDoubleSpinBox::valueChanged, [this](double v) {
            if (m_syncing || m_slider.isNull()) return;
            m_syncing = true;
            m_slider->blockSignals(true);
            m_slider->setValue(ToTick(static_cast<float>(v)));
            m_slider->blockSignals(false);
            m_syncing = false;
            if (onChanged) onChanged(static_cast<float>(v));
        });
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_slider.isNull() && !m_spin.isNull(); }

    void SetValue(const float& val) override {
        if (!IsValid()) return;
        m_spin->blockSignals(true);
        m_slider->blockSignals(true);
        m_spin->setValue(val);
        m_slider->setValue(ToTick(val));
        m_slider->blockSignals(false);
        m_spin->blockSignals(false);
    }

    // The spin box is authoritative: it holds the unquantised value, so a number
    // typed between two ticks survives being displayed on the slider.
    float GetValue() override {
        return m_spin.isNull() ? 0.0f : static_cast<float>(m_spin->value());
    }

private:
    int ToTick(float v) const {
        const float t = (v - m_min) / (m_max - m_min);
        return std::clamp(static_cast<int>(std::lround(t * m_ticks)), 0, m_ticks);
    }
    float FromTick(int tick) const {
        return m_min + (m_max - m_min) * (static_cast<float>(tick) / static_cast<float>(m_ticks));
    }

    float m_min, m_max, m_step;
    int   m_ticks = 1;
    bool  m_syncing = false;
    QWidget* m_container = nullptr;
    QPointer<QSlider> m_slider;
    QPointer<QDoubleSpinBox> m_spin;
};


// The two-handle bar behind RangeSliderPropertyWidget. Qt ships no two-handle
// slider, so this paints its own groove, span and handles and does its own hit
// testing -- there is no QStyle primitive to borrow, which is why the colours are
// literals here instead of living in default.qss like every other widget's.
// They are lifted from the stylesheet's existing palette (QProgressBar::chunk
// green, the checkbox hover green) so it still reads as part of the same theme.
//
// No Q_OBJECT: it reports changes through a std::function like the rest of this
// header's widgets, so it needs no moc pass.
class RangeSliderBar : public QWidget {
public:
    std::function<void()> onChanged;

    RangeSliderBar(float lo, float hi, float step, QWidget* parent = nullptr)
        : QWidget(parent), m_min(lo), m_max(hi), m_step(step > 0.0f ? step : 0.01f),
          m_low(lo), m_high(hi) {
        setFixedHeight(kHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
    }

    void SetSpan(float low, float high) {
        m_low  = std::clamp(low,  m_min, m_max);
        m_high = std::clamp(high, m_min, m_max);
        if (m_low > m_high) std::swap(m_low, m_high);
        update();
    }
    float Low()  const { return m_low; }
    float High() const { return m_high; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRect tr = Track();
        const int cy = tr.center().y();
        const int xLo = HandleX(m_low);
        const int xHi = HandleX(m_high);
        const bool on = isEnabled();

        // Groove, then the selected span over it.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(45, 45, 45));
        p.drawRoundedRect(QRect(tr.left(), cy - 2, tr.width(), 4), 2, 2);
        p.setBrush(on ? QColor(87, 126, 100) : QColor(70, 78, 72));
        p.drawRoundedRect(QRect(xLo, cy - 2, std::max(1, xHi - xLo), 4), 2, 2);

        // Values above their handles, each nudged inside the widget so neither
        // gets clipped at the ends. They may overlap when the span is nearly
        // empty, at which point both read the same number anyway.
        p.setPen(QColor(on ? 200 : 120, on ? 200 : 120, on ? 200 : 120));
        const QString loText = Format(m_low);
        const QString hiText = Format(m_high);
        const QFontMetrics fm = fontMetrics();
        auto drawLabel = [&](int x, const QString& text, bool rightOfHandle) {
            int w = fm.horizontalAdvance(text);
            int tx = rightOfHandle ? x - w : x;
            tx = std::clamp(tx, 0, std::max(0, width() - w));
            p.drawText(QRect(tx, 0, w, kTextH), Qt::AlignVCenter | Qt::AlignLeft, text);
        };
        drawLabel(xLo, loText, false);
        drawLabel(xHi, hiText, true);

        // Handles last so they sit above the span.
        for (int i = 0; i < 2; ++i) {
            const int x = i == 0 ? xLo : xHi;
            const bool active = on && (m_hover == i || m_active == i);
            p.setBrush(active ? QColor(131, 176, 145) : QColor(on ? 170 : 100, on ? 170 : 100, on ? 170 : 100));
            p.setPen(QPen(QColor(30, 30, 30), 1));
            p.drawEllipse(QPoint(x, cy), kHandleR, kHandleR);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (!isEnabled() || e->button() != Qt::LeftButton) return;
        m_active = NearestHandle(e->position().x());
        MoveActive(e->position().x());
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!isEnabled()) return;
        if (m_active >= 0) { MoveActive(e->position().x()); return; }
        const int h = NearestHandle(e->position().x());
        if (h != m_hover) { m_hover = h; update(); }
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_active = -1; update(); }
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }

private:
    static constexpr int kHeight  = 34;
    static constexpr int kTextH   = 14;
    static constexpr int kHandleR = 6;

    // Where the handle CENTRES may travel: inset by the handle radius so neither
    // circle is clipped at the ends of the widget.
    QRect Track() const {
        return QRect(kHandleR, kTextH, std::max(1, width() - 2 * kHandleR), height() - kTextH);
    }
    int HandleX(float v) const {
        const QRect tr = Track();
        const float t = (v - m_min) / (m_max - m_min);
        return tr.left() + static_cast<int>(std::lround(t * tr.width()));
    }
    float ValueAt(int x) const {
        const QRect tr = Track();
        const float t = static_cast<float>(x - tr.left()) / static_cast<float>(tr.width());
        return Snap(m_min + std::clamp(t, 0.0f, 1.0f) * (m_max - m_min));
    }
    float Snap(float v) const {
        const float snapped = m_min + std::round((v - m_min) / m_step) * m_step;
        return std::clamp(snapped, m_min, m_max);
    }
    // Whichever handle is closer, so a click anywhere on the bar grabs the one the
    // user plainly meant rather than requiring a hit on a 12px circle.
    int NearestHandle(int x) const {
        return std::abs(x - HandleX(m_low)) <= std::abs(x - HandleX(m_high)) ? 0 : 1;
    }
    void MoveActive(int x) {
        if (m_active < 0) return;
        const float v = ValueAt(x);
        // The handles cannot cross: dragging one past the other pins them equal,
        // which keeps x <= y true for every consumer of the pair.
        if (m_active == 0) m_low  = std::min(v, m_high);
        else               m_high = std::max(v, m_low);
        update();
        if (onChanged) onChanged();
    }
    QString Format(float v) const {
        return QString::number(static_cast<double>(v), 'f', DecimalsForStep(m_step));
    }

    float m_min, m_max, m_step, m_low, m_high;
    int m_active = -1;   // handle being dragged, -1 = none
    int m_hover  = -1;
};


// A (low, high) pair carried in a glm::vec2: x is the low end, y the high end.
// vec2 rather than a bespoke type because that is already how this engine spells
// a min/max pair -- see ParticleComponent's "Lifetime (min,max)" rows.
class RangeSliderPropertyWidget : public PropertyWidget<glm::vec2> {
public:
    explicit RangeSliderPropertyWidget(const Properties::PropDesc& desc) {
        m_bar = new RangeSliderBar(desc.min, desc.max, desc.step);
        if (IsReadOnly(desc)) m_bar->setEnabled(false);
        m_bar->onChanged = [this]() {
            if (onChanged) onChanged(GetValue());
        };
    }

    QWidget* GetWidget() override { return m_bar; }
    bool IsValid() override { return !m_bar.isNull(); }

    void SetValue(const glm::vec2& val) override {
        if (m_bar.isNull()) return;
        m_bar->SetSpan(val.x, val.y);
    }

    glm::vec2 GetValue() override {
        if (m_bar.isNull()) return glm::vec2(0.0f);
        return { m_bar->Low(), m_bar->High() };
    }

private:
    QPointer<RangeSliderBar> m_bar;
};


class Vec2PropertyWidget : public PropertyWidget<glm::vec2> {
public:
    explicit Vec2PropertyWidget(const Properties::PropDesc& desc) {
        container = new QWidget();
        container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        

        auto makeSpin = [&](const char* name, const char* labelText) {
            auto* lbl = new QLabel(labelText);
            auto font = lbl->font();
            font.setBold(true);
            lbl->setFont(font);
            layout->addWidget(lbl);

            auto* s = new QDoubleSpinBox();
            ConfigureSpinBox(s, desc);
            s->setObjectName(name);
            s->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            layout->addWidget(s);
            return s;
        };

        x = makeSpin("vec_x", "X");
        y = makeSpin("vec_y", "Y");

        QObject::connect(x, &QDoubleSpinBox::valueChanged, [this](double) {
            if (onChanged) onChanged(GetValue());
        });
        QObject::connect(y, &QDoubleSpinBox::valueChanged, [this](double) {
            if (onChanged) onChanged(GetValue());
        });
    }

    QWidget* GetWidget() override { return container; }
    bool IsValid() override { return !x.isNull() && !y.isNull(); }

    void SetValue(const glm::vec2& val) override {
        if (!IsValid()) return;
        x->blockSignals(true);
        y->blockSignals(true);
        x->setValue(val.x);
        y->setValue(val.y);
        x->blockSignals(false);
        y->blockSignals(false);
    }

    glm::vec2 GetValue() override {
        if (!IsValid()) return glm::vec2(0.0f);
        return { static_cast<float>(x->value()), static_cast<float>(y->value()) };
    }

private:
    QWidget* container = nullptr;
    QPointer<QDoubleSpinBox> x;
    QPointer<QDoubleSpinBox> y;
};


class Vec3PropertyWidget : public PropertyWidget<glm::vec3> {
public:
    explicit Vec3PropertyWidget(const Properties::PropDesc& desc) {
        container = new QWidget();
        container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);

        auto makeSpin = [&](const char* name, const char* labelText) {
            auto* lbl = new QLabel(labelText);
            auto font = lbl->font();
            font.setBold(true);
            lbl->setFont(font);
            layout->addWidget(lbl);

            auto* s = new QDoubleSpinBox();
            ConfigureSpinBox(s, desc);
            s->setObjectName(name);
            s->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            layout->addWidget(s);
            return s;
        };

        x = makeSpin("vec_x", "X");
        y = makeSpin("vec_y", "Y");
        z = makeSpin("vec_z", "Z");

        auto notify = [this](double) { if (onChanged) onChanged(GetValue()); };
        QObject::connect(x, &QDoubleSpinBox::valueChanged, notify);
        QObject::connect(y, &QDoubleSpinBox::valueChanged, notify);
        QObject::connect(z, &QDoubleSpinBox::valueChanged, notify);
    }

    QWidget* GetWidget() override { return container; }
    bool IsValid() override { return !x.isNull() && !y.isNull() && !z.isNull(); }

    void SetValue(const glm::vec3& val) override {
        if (!IsValid()) return;
        x->blockSignals(true); y->blockSignals(true); z->blockSignals(true);
        x->setValue(val.x); y->setValue(val.y); z->setValue(val.z);
        x->blockSignals(false); y->blockSignals(false); z->blockSignals(false);
    }

    glm::vec3 GetValue() override {
        if (!IsValid()) return glm::vec3(0.0f);
        return { static_cast<float>(x->value()), static_cast<float>(y->value()), static_cast<float>(z->value()) };
    }

private:
    QWidget* container = nullptr;
    QPointer<QDoubleSpinBox> x;
    QPointer<QDoubleSpinBox> y;
    QPointer<QDoubleSpinBox> z;
};


class Vec4PropertyWidget : public PropertyWidget<glm::vec4> {
public:
    explicit Vec4PropertyWidget(const Properties::PropDesc& desc) {
        container = new QWidget();
        container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);

        auto makeSpin = [&](const char* name, const char* labelText) {
            auto* lbl = new QLabel(labelText);
            auto font = lbl->font();
            font.setBold(true);
            lbl->setFont(font);
            layout->addWidget(lbl);

            auto* s = new QDoubleSpinBox();
            ConfigureSpinBox(s, desc);
            s->setObjectName(name);
            s->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            layout->addWidget(s);
            return s;
        };

        x = makeSpin("vec_x", "X");
        y = makeSpin("vec_y", "Y");
        z = makeSpin("vec_z", "Z");
        w = makeSpin("vec_w", "W");

        auto notify = [this](double) { if (onChanged) onChanged(GetValue()); };
        QObject::connect(x, &QDoubleSpinBox::valueChanged, notify);
        QObject::connect(y, &QDoubleSpinBox::valueChanged, notify);
        QObject::connect(z, &QDoubleSpinBox::valueChanged, notify);
        QObject::connect(w, &QDoubleSpinBox::valueChanged, notify);
    }

    QWidget* GetWidget() override { return container; }
    bool IsValid() override { return !x.isNull() && !y.isNull() && !z.isNull() && !w.isNull(); }

    void SetValue(const glm::vec4& val) override {
        if (!IsValid()) return;
        x->blockSignals(true); y->blockSignals(true); z->blockSignals(true); w->blockSignals(true);
        x->setValue(val.x); y->setValue(val.y); z->setValue(val.z); w->setValue(val.w);
        x->blockSignals(false); y->blockSignals(false); z->blockSignals(false); w->blockSignals(false);
    }

    glm::vec4 GetValue() override {
        if (!IsValid()) return glm::vec4(0.0f);
        return { static_cast<float>(x->value()), static_cast<float>(y->value()),
                 static_cast<float>(z->value()), static_cast<float>(w->value()) };
    }

private:
    QWidget* container = nullptr;
    QPointer<QDoubleSpinBox> x;
    QPointer<QDoubleSpinBox> y;
    QPointer<QDoubleSpinBox> z;
    QPointer<QDoubleSpinBox> w;
};


class BoolPropertyWidget : public PropertyWidget<bool> {
public:
    explicit BoolPropertyWidget(const Properties::PropDesc& desc) {
        checkbox = new QCheckBox();
        if (IsReadOnly(desc)) checkbox->setEnabled(false);

        QObject::connect(checkbox, &QCheckBox::toggled, [this](bool val) {
            if (onChanged) onChanged(val);
        });
    }

    QWidget* GetWidget() override { return checkbox; }
    bool IsValid() override { return !checkbox.isNull(); }

    void SetValue(const bool& val) override {
        if (checkbox.isNull()) return;
        checkbox->blockSignals(true);
        checkbox->setChecked(val);
        checkbox->blockSignals(false);
    }

    bool GetValue() override {
        return checkbox.isNull() ? false : checkbox->isChecked();
    }

private:
    QPointer<QCheckBox> checkbox;
};

class Vec4ColorPropertyWidget : public PropertyWidget<glm::vec4> {
public:
    explicit Vec4ColorPropertyWidget(const Properties::PropDesc& desc) {
        btn = new QPushButton();
        btn->setObjectName("ColorButton");
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // The swatch IS the control, so disabling it is the whole story — there is
        // no colour dialog to open and nothing else on the row to interact with.
        if (IsReadOnly(desc)) btn->setEnabled(false);

        QObject::connect(btn, &QPushButton::clicked, [this]() {
            glm::vec4 current = GetValue();
            QColor initial(
                static_cast<int>(current.r * 255),
                static_cast<int>(current.g * 255),
                static_cast<int>(current.b * 255),
                static_cast<int>(current.a * 255)
            );

            auto* dialog = new QColorDialog();
            dialog->setCurrentColor(initial);
            dialog->setOptions(QColorDialog::ShowAlphaChannel);
            dialog->setModal(false);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setOption(QColorDialog::NoButtons);
            dialog->setOption(QColorDialog::DontUseNativeDialog);
            dialog->setWindowFlags(Qt::Popup);

            QObject::connect(dialog, &QColorDialog::currentColorChanged,
                [this](const QColor& selected) {
                    if (!selected.isValid()) return;
                    glm::vec4 c(
                        static_cast<float>(selected.redF()),
                        static_cast<float>(selected.greenF()),
                        static_cast<float>(selected.blueF()),
                        static_cast<float>(selected.alphaF())
                    );
                    cachedColor = c;
                    UpdateButtonColor(c);
                    if (onChanged) onChanged(c);
                });

            dialog->show();
        });
    }

    QWidget* GetWidget() override { return btn; }
    bool IsValid() override { return !btn.isNull(); }

    void SetValue(const glm::vec4& val) override {
        cachedColor = val;
        UpdateButtonColor(val);
    }

    glm::vec4 GetValue() override { return cachedColor; }

private:
    void UpdateButtonColor(const glm::vec4& color) {
        if (btn.isNull()) return;
        QColor qcol(
            static_cast<int>(color.r * 255),
            static_cast<int>(color.g * 255),
            static_cast<int>(color.b * 255),
            static_cast<int>(color.a * 255)
        );
        btn->setStyleSheet(QString("background-color: %1;").arg(qcol.name()));
    }

    QPointer<QPushButton> btn;
    glm::vec4 cachedColor{1, 1, 1, 1};
};

class StringPropertyWidget : public PropertyWidget<std::string> {
public:
    explicit StringPropertyWidget(const Properties::PropDesc& desc) {
        edit = new QLineEdit();
        if (IsReadOnly(desc))
            edit->setReadOnly(true);

        QObject::connect(edit, &QLineEdit::textChanged, [this](const QString& text) {
            if (onChanged) onChanged(text.toStdString());
        });
    }

    QWidget* GetWidget() override { return edit; }
    bool IsValid() override { return !edit.isNull(); }

    void SetValue(const std::string& val) override {
        if (edit.isNull()) return;
        edit->blockSignals(true);
        edit->setText(QString::fromStdString(val));
        edit->blockSignals(false);
    }

    std::string GetValue() override {
        return edit.isNull() ? "" : edit->text().toStdString();
    }

private:
    QPointer<QLineEdit> edit;
};

// The drag handle in a ResizableTextEdit's bottom-right corner.
//
// A real child widget rather than something painted into the text area: once the
// vertical scrollbar appears it owns that corner, and only a widget raised above
// it keeps both the cursor shape and the drag alive. It also scopes the resize
// cursor to the handle instead of the whole field.
class TextBoxResizeGrip : public QWidget {
public:
    static constexpr int kSize = 14;

    TextBoxResizeGrip(QWidget* target, int minHeight, int maxHeight)
        : QWidget(target), m_target(target), m_minH(minHeight), m_maxH(maxHeight)
    {
        setFixedSize(kSize, kSize);
        setCursor(Qt::SizeVerCursor);
        setToolTip("Drag to resize");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        // The three diagonal ticks a browser draws on a <textarea>, so the
        // gesture is recognisable without having to find the tooltip first.
        QPainter p(this);
        p.setPen(QPen(QColor(120, 120, 120)));
        for (int i = 0; i < 3; ++i) {
            const int o = 3 + i * 4;
            p.drawLine(width() - o, height() - 2, width() - 2, height() - o);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton || !m_target) { QWidget::mousePressEvent(e); return; }
        m_dragging     = true;
        m_pressGlobalY = e->globalPosition().toPoint().y();
        m_pressHeight  = m_target->height();
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_dragging || !m_target) { QWidget::mouseMoveEvent(e); return; }
        // Tracked against the press point in global coordinates, not against the
        // last move: the grip moves with the widget it is resizing, so a delta
        // read from local coordinates would chase itself.
        const int dy = e->globalPosition().toPoint().y() - m_pressGlobalY;
        m_target->setFixedHeight(std::clamp(m_pressHeight + dy, m_minH, m_maxH));
        e->accept();
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        m_dragging = false;
        e->accept();
    }

private:
    QWidget* m_target = nullptr;
    int  m_minH = 0;
    int  m_maxH = 0;
    bool m_dragging = false;
    int  m_pressGlobalY = 0;
    int  m_pressHeight = 0;
};

// The multi-line editor behind TextBoxPropertyWidget.
//
// QPlainTextEdit rather than QTextEdit: the bound value is a plain std::string,
// and QTextEdit's rich-text machinery would accept pasted formatting only to
// throw it away on the way out.
//
// Height is user-adjustable by dragging the bottom-right grip, the way an HTML
// <textarea> resizes -- vertical only. Width belongs to the inspector's grid
// column: a field that set its own would either sit visibly narrower than every
// other row or widen the column and force a horizontal scrollbar on the whole
// panel. That is `resize: vertical`, which is where a textarea inside a form
// layout usually lands anyway.
class ResizableTextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ResizableTextEdit(int rows, QWidget* parent = nullptr)
        : QPlainTextEdit(parent)
    {
        // Ignored horizontally, like every other property widget, so the grid
        // column drives the width and a long paragraph can't stretch the panel.
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        // In a form, Tab is worth more as "next field" than as a literal tab
        // character. Ctrl+Tab still inserts one.
        setTabChangesFocus(true);

        m_grip = new TextBoxResizeGrip(this, HeightForRows(kMinRows), HeightForRows(kMaxRows));
        setFixedHeight(HeightForRows(rows));
    }

    static constexpr int kMinRows = 2;
    static constexpr int kMaxRows = 40;

    // Pixel height that fits `rows` lines of text. Derived from the current font
    // and the frame/document margins rather than a constant, so it survives a
    // stylesheet padding change or a different UI font.
    int HeightForRows(int rows) const {
        const QMargins cm = contentsMargins();
        return fontMetrics().lineSpacing() * rows
             + static_cast<int>(document()->documentMargin()) * 2
             + cm.top() + cm.bottom()
             + frameWidth() * 2;
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        if (m_grip) {
            m_grip->move(width()  - TextBoxResizeGrip::kSize - 1,
                         height() - TextBoxResizeGrip::kSize - 1);
            m_grip->raise();
        }
    }

private:
    TextBoxResizeGrip* m_grip = nullptr;
};

// Paragraph-friendly string property — what Tags::MULTILINE maps to. Same
// contract as StringPropertyWidget, just not confined to one line.
class TextBoxPropertyWidget : public PropertyWidget<std::string> {
public:
    static constexpr int kDefaultRows = 4;

    explicit TextBoxPropertyWidget(const Properties::PropDesc& desc) {
        edit = new ResizableTextEdit(kDefaultRows);
        // Read by InspectorVisitor::AddRow: a row this tall reads better with its
        // label pinned to the first line of text than floating at the midpoint.
        edit->setProperty("labelTopAlign", true);
        if (IsReadOnly(desc))
            edit->setReadOnly(true);

        QObject::connect(edit, &QPlainTextEdit::textChanged, [this]() {
            if (edit.isNull() || !onChanged) return;
            onChanged(edit->toPlainText().toStdString());
        });
    }

    QWidget* GetWidget() override { return edit; }
    bool IsValid() override { return !edit.isNull(); }

    void SetValue(const std::string& val) override {
        if (edit.isNull()) return;
        const QString next = QString::fromStdString(val);
        // A correctness guard, not an optimisation. SetValue also arrives as the
        // echo of the user's own keystroke -- the edit fires the property's
        // change event, which marks the row dirty, and the inspector's refresh
        // timer pushes the value straight back. setPlainText resets the caret to
        // the start of the document, so without this every character typed
        // mid-paragraph would fling the cursor to the top.
        if (edit->toPlainText() == next) return;
        edit->blockSignals(true);
        edit->setPlainText(next);
        edit->blockSignals(false);
    }

    std::string GetValue() override {
        return edit.isNull() ? "" : edit->toPlainText().toStdString();
    }

private:
    QPointer<ResizableTextEdit> edit;
};

// A QLineEdit that emits clicked() on mouse press — used by ObjectRefPropertyWidget.
class ClickableLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit ClickableLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {
        setCursor(Qt::PointingHandCursor);
    }
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent* e) override {
        emit clicked();
        QLineEdit::mousePressEvent(e);
    }
};

// A frameless, click-through popup that fades in above a ref field and shows the
// referenced asset's thumbnail — a hover preview replacing the old always-visible
// collapsible thumbnail. Width follows the anchor field; height is fixed.
class AssetPreviewPopup : public QWidget {
public:
    static constexpr int kHeight = 96;   // matches the old inline thumbnail height

    explicit AssetPreviewPopup(QWidget* parent = nullptr)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);  // never steal the hover
        setAttribute(Qt::WA_ShowWithoutActivating);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_label = new QLabel(this);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setStyleSheet("background-color: #1a1a1a; border: 1px solid #3a3a3a;");
        layout->addWidget(m_label);

        m_fade = new QPropertyAnimation(this, "windowOpacity", this);
        m_fade->setDuration(130);
        QObject::connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
            if (windowOpacity() <= 0.01) hide();   // only a completed fade-out hides
        });
    }

    void showAbove(QWidget* anchor, const QPixmap& px) {
        if (!anchor || px.isNull()) return;
        const int w = anchor->width();
        resize(w, kHeight);
        m_label->setPixmap(px.scaled(w, kHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        const QPoint tl = anchor->mapToGlobal(QPoint(0, 0));
        move(tl.x(), tl.y() - kHeight - 4);   // float just above the field
        if (!isVisible()) setWindowOpacity(0.0);
        show();
        raise();
        m_fade->stop();
        m_fade->setStartValue(windowOpacity());
        m_fade->setEndValue(1.0);
        m_fade->start();
    }

    void fadeOut() {
        if (!isVisible()) return;
        m_fade->stop();
        m_fade->setStartValue(windowOpacity());
        m_fade->setEndValue(0.0);
        m_fade->start();
    }

private:
    QLabel* m_label = nullptr;
    QPropertyAnimation* m_fade = nullptr;
};

// Event filter that drives an AssetPreviewPopup from hover over a ref field (and
// its child edit/button). Leaving to a child of the anchor keeps the preview up;
// leaving the anchor's rect entirely fades it out.
class AssetHoverFilter : public QObject {
public:
    AssetHoverFilter(QWidget* anchor, AssetPreviewPopup* popup,
                     std::function<QPixmap()> thumb, QObject* parent)
        : QObject(parent), m_anchor(anchor), m_popup(popup), m_thumb(std::move(thumb)) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Enter) {
            if (m_popup && m_anchor) m_popup->showAbove(m_anchor, m_thumb ? m_thumb() : QPixmap());
        } else if (event->type() == QEvent::Leave) {
            // Moving onto a child of the field still counts as hovering it.
            if (m_anchor && m_popup) {
                const QPoint local = m_anchor->mapFromGlobal(QCursor::pos());
                if (!m_anchor->rect().contains(local)) m_popup->fadeOut();
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget* m_anchor;
    AssetPreviewPopup* m_popup;
    std::function<QPixmap()> m_thumb;
};

class ObjectRefPropertyWidget : public PropertyWidget<std::string> {
public:
    explicit ObjectRefPropertyWidget(const Properties::PropDesc& desc) : m_desc(desc) {
        m_container = new QWidget();
        m_container->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        m_edit = new ClickableLineEdit();
        m_edit->setReadOnly(true);
        m_edit->setObjectName("ObjectRefEdit");
        m_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(m_edit);

        // Search icon in place of the old ellipsis glyph. Resolved through GetAssetPath
        // so it works from a bundled build as well as a dev run, matching how the
        // picker's own fallback icons below are loaded.
        m_btn = new QPushButton();
        m_btn->setIcon(QIcon(QString::fromStdString(
            EngineUtils::GetAssetPath("Domain/lib/assets/icons/search_icon.png"))));
        // Set explicitly: the style's default icon size is unrelated to this button's
        // fixed 26px width, and the source PNG is far larger than it renders at.
        m_btn->setIconSize(QSize(14, 14));
        // The button carries no text now, so name what it does for hover/accessibility.
        m_btn->setToolTip("Browse");
        m_btn->setFixedWidth(26);
        m_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        layout->addWidget(m_btn);

        // A display-only reference keeps its name field (and the hover preview
        // below) but loses every way to reassign it: the picker is disabled rather
        // than removed so the row keeps the same shape as its editable siblings,
        // and neither entry point into openPicker() is connected. Note m_edit is
        // ALWAYS setReadOnly above -- that stops typing into the name, which is
        // display formatting, not the property being read-only.
        const bool readOnly = IsReadOnly(desc);
        if (readOnly) {
            m_btn->setEnabled(false);
            m_btn->setToolTip("Read-only");
            // ClickableLineEdit claims the pointing-hand cursor in its constructor,
            // which would keep advertising a click that no longer does anything.
            m_edit->setCursor(Qt::IBeamCursor);
        } else {
            QObject::connect(m_btn, &QPushButton::clicked, [this]() { openPicker(); });
            QObject::connect(m_edit, &ClickableLineEdit::clicked, [this]() { openPicker(); });
        }

        // Drag & drop: accept a compatible Hierarchy GameObject / Folder-view
        // asset dropped onto this row (see RefDropFilter). The read-only line
        // edit would otherwise swallow drops, so let them fall through to the
        // container that hosts the filter. Skipped entirely when display-only --
        // a drop is an edit, and it would bypass the disabled picker.
        m_edit->setAcceptDrops(false);
        if (!readOnly) {
            m_container->setAcceptDrops(true);
            m_container->installEventFilter(new RefDropFilter(m_desc, [this](const std::string& id) {
                SetValue(id);
                if (onChanged) onChanged(id);
            }, m_container));
        }

        // Ref fields with a previewable thumbnail get a hover preview: a popup that
        // fades in above the field (replacing the old always-on collapsible
        // thumbnail). Installed on the field and its children so moving between them
        // doesn't drop the hover. GameObject refs preview their sprite when present;
        // currentThumbnail() returns empty otherwise, so those simply show nothing.
        if (m_desc.tag == Properties::Tags::TEXTURE    ||
            m_desc.tag == Properties::Tags::SPRITE     ||
            m_desc.tag == Properties::Tags::MATERIAL   ||
            m_desc.tag == Properties::Tags::OBJECT_REF) {
            m_previewPopup = new AssetPreviewPopup(m_container);
            auto* hover = new AssetHoverFilter(
                m_container, m_previewPopup,
                [this]() { return currentThumbnail(); }, m_container);
            m_container->installEventFilter(hover);
            m_edit->installEventFilter(hover);
            m_btn->installEventFilter(hover);
        }
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_edit.isNull(); }

    // Stores the ID internally; displays the resolved name in the text field.
    void SetValue(const std::string& id) override {
        if (m_edit.isNull()) return;
        m_currentId = id;
        m_edit->blockSignals(true);
        m_edit->setText(QString::fromStdString(nameForId(id)));
        m_edit->blockSignals(false);
    }

    // Always returns the ID, not the display name.
    std::string GetValue() override { return m_currentId; }

private:
    void openPicker() {
        auto items = buildItems();

        std::function<QPixmap(const std::string&)> thumbGen;
        if (m_desc.tag == Properties::Tags::MATERIAL)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forMaterial(id); };
        else if (m_desc.tag == Properties::Tags::SPRITE)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forSprite(id); };
        else if (m_desc.tag == Properties::Tags::TEXTURE)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forTexture(id); };
        else if (m_desc.tag == Properties::Tags::SCRIPT)
            // Scripts have no visual thumbnail — the fallback file icon is used.
            thumbGen = nullptr;
        else
            // Generic OBJECT_REF (game objects): preview the object's SpriteRenderer sprite.
            thumbGen = [this](const std::string& id) -> QPixmap {
                auto* go = findGameObject(id);
                if (!go) return {};
                auto* sr = go->GetComponent<SpriteRenderer>();
                if (!sr || sr->GetSpriteID().empty()) return {};
                return AssetThumbnails::forSprite(sr->GetSpriteID());
            };

        // Fallback: use CustomIconProvider on the asset's file path.
        // Gives shader cells the shader icon, .material/.sprite files the OS icon, etc.
        auto iconFromFilePath = [](const std::string& fp) -> QIcon {
            if (fp.empty()) return {};
            EditorUtils::CustomIconProvider prov;
            return prov.icon(QFileInfo(QString::fromStdString(fp)));
        };

        std::function<QIcon(const std::string&)> fallbackIconGen;
        if (m_desc.tag == Properties::Tags::MATERIAL)
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetMaterial(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::SPRITE)
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetSprite(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::TEXTURE)
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetTexture(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::SHADER)
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetShader(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::FONT)
            // No thumbnail generator above, so the picker falls back to this: the
            // OS icon for the .font meta. A rendered specimen would be nicer but
            // needs a GL context the picker doesn't have.
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetFont(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::AUDIO_CLIP)
            // No thumbnail generator (no waveform preview) -- falls back to the OS
            // icon for the .audio meta, same as FONT above.
            fallbackIconGen = [iconFromFilePath](const std::string& id) {
                auto* a = AssetManager::Get().GetAudioClip(id);
                return a ? iconFromFilePath(a->GetFilePath()) : QIcon{};
            };
        else if (m_desc.tag == Properties::Tags::SCRIPT)
            fallbackIconGen = [iconFromFilePath](const std::string& id) -> QIcon {
                // id is "module:class" — icon the module's .py file.
                const std::string module = id.substr(0, id.find(':'));
                if (module.empty()) return {};
                return iconFromFilePath(
                    EngineUtils::GetAssetPath("Domain/sandbox/scripts/" + module + ".py"));
            };
        else
            // Game objects have no file path (and may have no sprite) — fall back
            // to the generic game-object (cube) icon.
            fallbackIconGen = [](const std::string&) -> QIcon {
                return EditorUtils::CustomIconProvider::gameObjectIcon();
            };

        auto* picker = new AssetPickerWidget(std::move(items), std::move(thumbGen), std::move(fallbackIconGen), m_container);
        picker->onSelected = [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        };
        picker->setSelectedId(m_currentId);   // highlight + scroll to the current value
        auto pos = m_container->rect().topLeft();
        pos.setX(pos.x() - picker->width());
        picker->showAt(m_container->mapToGlobal(pos));
    }

    // Resolves a human-readable name for an ID based on the widget's tag type.
    // Falls back to the raw ID if the asset/object isn't found.
    std::string nameForId(const std::string& id) const {
        if (id.empty()) return "";
        auto& am = AssetManager::Get();
        if (m_desc.tag == Properties::Tags::MATERIAL) {
            auto* a = am.GetMaterial(id); return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::SPRITE) {
            auto* a = am.GetSprite(id);   return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::TEXTURE) {
            auto* a = am.GetTexture(id);  return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::SHADER) {
            auto* a = am.GetShader(id);   return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::FONT) {
            auto* a = am.GetFont(id);     return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::AUDIO_CLIP) {
            auto* a = am.GetAudioClip(id); return a ? a->GetName() : id;
        } else if (m_desc.tag == Properties::Tags::SCRIPT) {
            // id is "module:class" — show the class name (what the user picked).
            const auto colon = id.find(':');
            return colon == std::string::npos ? id : id.substr(colon + 1);
        } else {
            auto* go = findGameObject(id);
            return go ? go->GetName() : id;
        }
    }

    // Thumbnail for the current value's hover preview. Empty (→ no preview) for
    // non-previewable tags, no value, or a GameObject with no sprite to show.
    QPixmap currentThumbnail() const {
        if (m_currentId.empty()) return {};
        if (m_desc.tag == Properties::Tags::TEXTURE)  return AssetThumbnails::forTexture(m_currentId);
        if (m_desc.tag == Properties::Tags::SPRITE)   return AssetThumbnails::forSprite(m_currentId);
        if (m_desc.tag == Properties::Tags::MATERIAL) return AssetThumbnails::forMaterial(m_currentId);
        if (m_desc.tag == Properties::Tags::OBJECT_REF) {
            // Preview a referenced GameObject via its SpriteRenderer's sprite, when
            // it has one (same source the picker uses for object thumbnails).
            auto* go = findGameObject(m_currentId);
            if (!go) return {};
            auto* sr = go->GetComponent<SpriteRenderer>();
            if (!sr || sr->GetSpriteID().empty()) return {};
            return AssetThumbnails::forSprite(sr->GetSpriteID());
        }
        return {};
    }

    // Looks up a GameObject by ID across all loaded scenes; nullptr if not found.
    GameObject* findGameObject(const std::string& id) const {
        if (id.empty()) return nullptr;
        auto* container = Engine::Get()->GetActiveContainer();
        if (!container) return nullptr;
        auto* sm = container->FindSystem<SceneManager>();
        if (!sm) return nullptr;
        for (auto* scene : sm->GetScenes())
            for (auto* go : scene->GetAllGameObjects())
                if (go->GetID() == id) return go;
        return nullptr;
    }

    std::vector<std::pair<std::string, std::string>> buildItems() {
        std::vector<std::pair<std::string, std::string>> items;
        auto& am = AssetManager::Get();

        if (m_desc.tag == Properties::Tags::MATERIAL) {
            for (const auto& [id, mat] : am.GetAllMaterials())
                items.push_back({mat->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::SPRITE) {
            for (const auto& [id, spr] : am.GetAllSprites())
                items.push_back({spr->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::TEXTURE) {
            for (const auto& [id, tex] : am.GetAllTextures())
                items.push_back({tex->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::SHADER) {
            for (const auto& [id, sh] : am.GetAllShaders())
                items.push_back({sh->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::FONT) {
            for (const auto& [id, f] : am.GetAllFonts())
                items.push_back({f->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::AUDIO_CLIP) {
            for (const auto& [id, clip] : am.GetAllAudioClips())
                items.push_back({clip->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::SCRIPT) {
            // Every ScriptableComponent subclass, keyed by "module:class".
            // Disambiguate a class name shared across modules as "Class (module)".
            auto available = ScriptComponent::GetAvailableScripts();
            std::map<std::string, int> classCounts;
            for (const auto& s : available) classCounts[s.className]++;
            for (const auto& s : available) {
                std::string display = classCounts[s.className] > 1
                    ? s.className + " (" + s.moduleName + ")"
                    : s.className;
                items.push_back({display, s.moduleName + ":" + s.className});
            }
        } else {
            // Generic OBJECT_REF — enumerate all GameObjects, optionally filtered
            // by script class (gameobject:<Class>) or native component type
            // (component:<Type>, e.g. a Camera field).
            auto* container = Engine::Get()->GetActiveContainer();
            if (!container) return items;
            auto* sm = container->FindSystem<SceneManager>();
            if (!sm) return items;
            for (auto* scene : sm->GetScenes()) {
                for (auto* go : scene->GetAllGameObjects()) {
                    if (!m_desc.refClassFilter.empty()) {
                        auto* sc = go->GetComponent<ScriptComponent>();
                        if (!sc || sc->GetScriptClassName() != m_desc.refClassFilter)
                            continue;
                    }
                    if (!m_desc.componentTypeFilter.empty() &&
                        !go->HasComponentByName(m_desc.componentTypeFilter))
                        continue;
                    items.push_back({go->GetName(), go->GetID()});
                }
            }
        }

        std::sort(items.begin(), items.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return items;
    }

    Properties::PropDesc m_desc;
    std::string m_currentId;
    QWidget* m_container = nullptr;
    QPointer<ClickableLineEdit> m_edit;
    QPointer<QPushButton> m_btn;
    AssetPreviewPopup* m_previewPopup = nullptr;   // hover preview (asset tags only)
};

// Container that keeps a name-overlay label (and collapse button) pinned to its
// full bottom edge on resize.
class ThumbContainer : public QWidget {
public:
    QLabel* overlay = nullptr;
    QWidget* collapseBtn = nullptr;
    static constexpr int overlayH = 24;
    static constexpr int collapseBtnW = 22;
    explicit ThumbContainer(QWidget* parent = nullptr) : QWidget(parent) {}
protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (collapseBtn)
            collapseBtn->setGeometry(0, height() - overlayH, collapseBtnW, overlayH);
        if (overlay)
            overlay->setGeometry(collapseBtnW, height() - overlayH,
                                 width() - collapseBtnW, overlayH);
    }
};

class AssetPreviewPropertyWidget : public PropertyWidget<std::string> {
public:
    static constexpr int kThumbH = 96;

    explicit AssetPreviewPropertyWidget(const Properties::PropDesc& desc) : m_desc(desc) {
        auto* tc = new ThumbContainer();
        tc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_container = tc;

        auto* layout = new QHBoxLayout(m_container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_thumb = new EditorUtils::ClickableLabel();
        m_thumb->setFixedHeight(kThumbH);
        m_thumb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_thumb->setAlignment(Qt::AlignCenter);
        m_thumb->setStyleSheet("background-color: #1a1a1a; border: 1px solid #3a3a3a;");
        layout->addWidget(m_thumb);

        m_nameLabel = new EditorUtils::ClickableLabel(m_container);
        m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        // Match the QLineEdit / QComboBox palette (grey, not near-black overlay);
        // lighten the background on hover to flag that the name is clickable.
        m_nameLabel->setStyleSheet(
            "QLabel { background-color: rgb(30, 30, 30); color: rgb(220, 220, 220);"
            " border: 1px solid rgb(51, 51, 51); border-left: none;"
            " font-size: padding: 0 2px; }"
            "QLabel:hover { background-color: rgb(52, 52, 52); color: white; }");
        m_nameLabel->raise();
        tc->overlay = m_nameLabel;

        // Clicking the name (not the thumbnail) jumps the Folder view to the asset.
        QObject::connect(m_nameLabel, &EditorUtils::ClickableLabel::clicked,
                         [this]() { revealInFolderView(); });

        // Collapse toggle sits at the left end of the name bar, right before the
        // label; hides the thumbnail and shrinks the widget down to just the name bar.
        m_collapseBtn = new QPushButton(QString(QChar(0x25B2)), m_container);  // ▲ up triangle
        m_collapseBtn->setCursor(Qt::PointingHandCursor);
        m_collapseBtn->setToolTip("Collapse");
        m_collapseBtn->setFocusPolicy(Qt::NoFocus);
        m_collapseBtn->setStyleSheet(
            "QPushButton { background-color: rgb(30, 30, 30); color: rgb(220, 220, 220);"
            " border: 1px solid rgb(51, 51, 51); border-right: none;"
            " font-size: 11px; padding: 0; }"
            "QPushButton:hover { background-color: rgb(52, 52, 52); }");
        m_collapseBtn->raise();
        tc->collapseBtn = m_collapseBtn;
        QObject::connect(m_collapseBtn, &QPushButton::clicked, [this]() { setCollapsed(!m_collapsed); });

        QObject::connect(m_thumb, &EditorUtils::ClickableLabel::clicked, [this]() { openPicker(); });

        // Drag & drop: accept a matching-type asset dragged from the Folder view
        // (material onto a material slot, sprite onto a sprite slot, etc.). The
        // child labels don't accept drops, so events reach the container filter.
        m_container->setAcceptDrops(true);
        m_container->installEventFilter(new RefDropFilter(m_desc, [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        }, m_container));

        // Start collapsed: show just the name bar until the user expands it.
        setCollapsed(true);
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_thumb.isNull(); }

    void SetValue(const std::string& id) override {
        if (m_thumb.isNull()) return;
        m_currentId = id;
        if (!m_nameLabel.isNull()) {
            std::string name = id;
            if (!id.empty()) {
                auto& am = AssetManager::Get();
                if (m_desc.tag == Properties::Tags::TEXTURE) {
                    if (auto* a = am.GetTexture(id))  name = a->GetName();
                } else if (m_desc.tag == Properties::Tags::SPRITE) {
                    if (auto* a = am.GetSprite(id))   name = a->GetName();
                } else if (m_desc.tag == Properties::Tags::MATERIAL) {
                    if (auto* a = am.GetMaterial(id)) name = a->GetName();
                }
            }
            QFontMetrics fm(m_nameLabel->font());
            // Left-aligned text: reserve room on the right for the collapse button
            // (plus the label's 2px side padding) so it elides with "…" before
            // running under the button.
            m_nameLabel->setText(
                fm.elidedText(QString::fromStdString(name), Qt::ElideRight,
                              m_container->width() - ThumbContainer::collapseBtnW - 6));
        }
        refreshThumb();
    }

    std::string GetValue() override { return m_currentId; }

private:
    // Toggles between the full thumbnail and a collapsed name-only bar.
    void setCollapsed(bool collapsed) {
        if (m_collapsed == collapsed) return;
        m_collapsed = collapsed;
        if (!m_thumb.isNull())
            m_thumb->setVisible(!collapsed);
        if (m_container)
            m_container->setFixedHeight(collapsed ? ThumbContainer::overlayH : kThumbH);
        if (!m_collapseBtn.isNull()) {
            m_collapseBtn->setText(QString(QChar(collapsed ? 0x25BC : 0x25B2)));  // ▼ / ▲
            m_collapseBtn->setToolTip(collapsed ? "Expand" : "Collapse");
        }
    }

    // Jumps the Folder view to the directory holding the current asset's source file.
    void revealInFolderView() {
        if (m_currentId.empty()) return;
        std::string fp;
        auto& am = AssetManager::Get();
        if (m_desc.tag == Properties::Tags::TEXTURE) {
            if (auto* a = am.GetTexture(m_currentId))  fp = a->GetFilePath();
        } else if (m_desc.tag == Properties::Tags::SPRITE) {
            if (auto* a = am.GetSprite(m_currentId))   fp = a->GetFilePath();
        } else if (m_desc.tag == Properties::Tags::MATERIAL) {
            if (auto* a = am.GetMaterial(m_currentId)) fp = a->GetFilePath();
        }
        if (!fp.empty())
            FolderViewGui::Get()->Navigate(fp);
    }

    void refreshThumb() {
        if (m_thumb.isNull()) return;
        QPixmap px;
        if (!m_currentId.empty()) {
            if (m_desc.tag == Properties::Tags::TEXTURE)
                px = AssetThumbnails::forTexture(m_currentId);
            else if (m_desc.tag == Properties::Tags::SPRITE)
                px = AssetThumbnails::forSprite(m_currentId);
            else if (m_desc.tag == Properties::Tags::MATERIAL)
                px = AssetThumbnails::forMaterial(m_currentId);
        }
        if (!px.isNull())
            m_thumb->setPixmap(px.scaled(m_thumb->width(), kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            m_thumb->clear();
    }

    void openPicker() {
        auto items = buildItems();

        std::function<QPixmap(const std::string&)> thumbGen;
        if (m_desc.tag == Properties::Tags::MATERIAL)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forMaterial(id); };
        else if (m_desc.tag == Properties::Tags::SPRITE)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forSprite(id); };
        else if (m_desc.tag == Properties::Tags::TEXTURE)
            thumbGen = [](const std::string& id) { return AssetThumbnails::forTexture(id); };

        auto iconFromFilePath = [](const std::string& fp) -> QIcon {
            if (fp.empty()) return {};
            EditorUtils::CustomIconProvider prov;
            return prov.icon(QFileInfo(QString::fromStdString(fp)));
        };
        std::function<QIcon(const std::string&)> fallbackIconGen =
            [this, iconFromFilePath](const std::string& id) -> QIcon {
                std::string fp;
                auto& am = AssetManager::Get();
                if (m_desc.tag == Properties::Tags::TEXTURE) {
                    auto* a = am.GetTexture(id); if (a) fp = a->GetFilePath();
                } else if (m_desc.tag == Properties::Tags::SPRITE) {
                    auto* a = am.GetSprite(id);  if (a) fp = a->GetFilePath();
                } else if (m_desc.tag == Properties::Tags::MATERIAL) {
                    auto* a = am.GetMaterial(id); if (a) fp = a->GetFilePath();
                }
                return iconFromFilePath(fp);
            };

        auto* picker = new AssetPickerWidget(std::move(items), std::move(thumbGen), std::move(fallbackIconGen), m_container);
        picker->onSelected = [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        };
        picker->setSelectedId(m_currentId);   // highlight + scroll to the current value
        auto pos = m_container->rect().topLeft();
        pos.setX(pos.x() - picker->width());
        pos.setY(pos.y() - picker->height()/2);
        picker->showAt(m_container->mapToGlobal(pos));
    }

    std::vector<std::pair<std::string, std::string>> buildItems() {
        std::vector<std::pair<std::string, std::string>> items;
        auto& am = AssetManager::Get();
        if (m_desc.tag == Properties::Tags::MATERIAL)
            for (const auto& [id, mat] : am.GetAllMaterials())
                items.push_back({mat->GetName(), id});
        else if (m_desc.tag == Properties::Tags::SPRITE)
            for (const auto& [id, spr] : am.GetAllSprites())
                items.push_back({spr->GetName(), id});
        else if (m_desc.tag == Properties::Tags::TEXTURE)
            for (const auto& [id, tex] : am.GetAllTextures())
                items.push_back({tex->GetName(), id});
        std::sort(items.begin(), items.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return items;
    }

    Properties::PropDesc m_desc;
    std::string m_currentId;
    QWidget* m_container = nullptr;
    QPointer<EditorUtils::ClickableLabel> m_thumb;
    QPointer<EditorUtils::ClickableLabel> m_nameLabel;
    QPointer<QPushButton> m_collapseBtn;
    bool m_collapsed = false;
};

class DropdownPropertyWidget : public PropertyWidget<int> {
public:
    explicit DropdownPropertyWidget(const Properties::PropDesc& desc) {
        combo = new QComboBox();
        combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        // A non-editable QComboBox has no read-only mode — setReadOnly exists only
        // on the line edit an editable one owns, and this one has none.
        if (IsReadOnly(desc)) combo->setEnabled(false);

        for (const auto& [label, value] : desc.dropdownOptions) {
            combo->addItem(QString::fromStdString(label), std::any_cast<int>(value));
        }

        QObject::connect(combo, &QComboBox::currentIndexChanged, [this](int index) {
            if (index < 0 || combo.isNull()) return;
            int val = combo->itemData(index).toInt();
            if (onChanged) onChanged(val);
        });
    }

    QWidget* GetWidget() override { return combo; }
    bool IsValid() override { return !combo.isNull(); }

    void SetValue(const int& val) override {
        if (combo.isNull()) return;
        combo->blockSignals(true);
        int index = combo->findData(val);
        if (index >= 0) combo->setCurrentIndex(index);
        combo->blockSignals(false);
    }

    int GetValue() override {
        if (combo.isNull() || combo->currentIndex() < 0) return 0;
        return combo->itemData(combo->currentIndex()).toInt();
    }

private:
    QPointer<QComboBox> combo;
};

// A dropdown whose value IS the chosen label, for str fields carrying
// Reflect[str, Options(...)].
//
// DropdownPropertyWidget above is a PropertyWidget<int>, so a scalar str field
// reaches it by converting label <-> index in its getter/setter (see
// InspectorVisitor). A list ELEMENT has no such layer to hide in -- its rows are
// bound as std::string by ListPropertyWidget<std::string> -- so the widget itself
// has to speak strings.
//
// Unlike its int sibling, a value matching no option shows blank rather than
// snapping to the first entry: a row added with +Add starts as an empty string,
// and "nothing picked yet" is the honest reading of that.
class StringDropdownPropertyWidget : public PropertyWidget<std::string> {
public:
    explicit StringDropdownPropertyWidget(const Properties::PropDesc& desc) {
        combo = new QComboBox();
        combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        if (IsReadOnly(desc)) combo->setEnabled(false);

        for (const auto& [label, value] : desc.dropdownOptions)
            combo->addItem(QString::fromStdString(label));

        QObject::connect(combo, &QComboBox::currentIndexChanged, [this](int index) {
            if (index < 0 || combo.isNull()) return;
            if (onChanged) onChanged(combo->itemText(index).toStdString());
        });
    }

    QWidget* GetWidget() override { return combo; }
    bool IsValid() override { return !combo.isNull(); }

    void SetValue(const std::string& val) override {
        if (combo.isNull()) return;
        combo->blockSignals(true);
        combo->setCurrentIndex(combo->findText(QString::fromStdString(val)));
        combo->blockSignals(false);
    }

    std::string GetValue() override {
        if (combo.isNull() || combo->currentIndex() < 0) return {};
        return combo->currentText().toStdString();
    }

private:
    QPointer<QComboBox> combo;
};
