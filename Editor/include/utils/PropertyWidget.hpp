#pragma once
#include <functional>
#include <QWidget>
#include <QPointer>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QStyle>
#include <QApplication>
#include <glm/glm.hpp>
#include "engine/utils/Properties.hpp"


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

class FloatPropertyWidget : public PropertyWidget<float> {
public:
    explicit FloatPropertyWidget(const Properties::PropDesc& desc) {
        spin = new QDoubleSpinBox();
        spin->setRange(desc.min, desc.max);
        spin->setSingleStep(desc.step);
        spin->setDecimals(2);
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
            s->setRange(desc.min, desc.max);
            s->setSingleStep(desc.step);
            s->setObjectName(name);
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

class BoolPropertyWidget : public PropertyWidget<bool> {
public:
    explicit BoolPropertyWidget(const Properties::PropDesc&) {
        checkbox = new QCheckBox();

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
    explicit Vec4ColorPropertyWidget(const Properties::PropDesc&) {
        btn = new QPushButton();
        btn->setObjectName("ColorButton");
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

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
    explicit StringPropertyWidget(const Properties::PropDesc&) {
        edit = new QLineEdit();

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

class ObjectRefPropertyWidget : public PropertyWidget<std::string> {
public:
    explicit ObjectRefPropertyWidget(const Properties::PropDesc& desc) {
        edit = new QLineEdit();
        edit->setReadOnly(true);
        edit->setObjectName("ObjectRefEdit");
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

class DropdownPropertyWidget : public PropertyWidget<int> {
public:
    explicit DropdownPropertyWidget(const Properties::PropDesc& desc) {
        combo = new QComboBox();
        combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

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
