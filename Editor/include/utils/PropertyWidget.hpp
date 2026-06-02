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
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QStyle>
#include <QApplication>
#include <QMouseEvent>
#include <glm/glm.hpp>
#include "engine/utils/Properties.hpp"
#include "utils/AssetPickerWidget.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ScriptComponent.hpp"


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
            s->setRange(desc.min, desc.max);
            s->setSingleStep(desc.step);
            s->setObjectName(name);
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
            s->setRange(desc.min, desc.max);
            s->setSingleStep(desc.step);
            s->setObjectName(name);
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

        m_btn = new QPushButton("\u2026");  // ellipsis "…"
        m_btn->setFixedWidth(26);
        m_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        layout->addWidget(m_btn);

        QObject::connect(m_btn, &QPushButton::clicked, [this]() { openPicker(); });
        QObject::connect(m_edit, &ClickableLineEdit::clicked, [this]() { openPicker(); });
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_edit.isNull(); }

    void SetValue(const std::string& val) override {
        if (m_edit.isNull()) return;
        m_edit->blockSignals(true);
        m_edit->setText(QString::fromStdString(val));
        m_edit->blockSignals(false);
    }

    std::string GetValue() override {
        return m_edit.isNull() ? "" : m_edit->text().toStdString();
    }

private:
    void openPicker() {
        auto items = buildItems();
        auto* picker = new AssetPickerWidget(std::move(items), m_container);
        picker->onSelected = [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        };
        auto pos = m_container->rect().topLeft();
        pos.setX(pos.x() - picker->width());
        picker->move(m_container->mapToGlobal(pos));
        picker->show();
    }

    std::vector<std::pair<std::string, std::string>> buildItems() {
        std::vector<std::pair<std::string, std::string>> items;

        if (m_desc.tag == Properties::Tags::MATERIAL) {
            for (const auto& [id, mat] : SharedResources::Get().GetAllMaterials())
                items.push_back({mat->GetName(), id});
        } else if (m_desc.tag == Properties::Tags::SPRITE) {
            for (const auto& [id, spr] : SharedResources::Get().GetAllSprites())
                items.push_back({spr->GetName(), id});
        } else {
            // Generic OBJECT_REF — enumerate all GameObjects, optionally filtered by script class
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
                    items.push_back({go->GetName(), go->GetID()});
                }
            }
        }

        std::sort(items.begin(), items.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return items;
    }

    Properties::PropDesc m_desc;
    QWidget* m_container = nullptr;
    QPointer<ClickableLineEdit> m_edit;
    QPointer<QPushButton> m_btn;
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
