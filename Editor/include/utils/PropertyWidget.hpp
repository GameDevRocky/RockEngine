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
#include "utils/AssetThumbnails.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/ScriptComponent.hpp"
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

class FloatPropertyWidget : public PropertyWidget<float> {
public:
    explicit FloatPropertyWidget(const Properties::PropDesc& desc) {
        spin = new QDoubleSpinBox();
        spin->setRange(desc.min, desc.max);
        spin->setSingleStep(desc.step);
        spin->setDecimals(desc.tag == Properties::Tags::INT ? 0 : 2);
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

        // PropDesc::tag holds a single value, so Tags::READONLY and a type tag such
        // as Tags::FLOAT are mutually exclusive. Accept either signal so a caller can
        // keep its type tag and still be display-only via PropDesc::ReadOnly().
        if (desc.readOnly || desc.tag == Properties::Tags::READONLY) {
            spin->setReadOnly(true);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        }

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
            s->setRange(desc.min, desc.max);
            s->setSingleStep(desc.step);
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
            s->setRange(desc.min, desc.max);
            s->setSingleStep(desc.step);
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
    explicit StringPropertyWidget(const Properties::PropDesc& desc) {
        edit = new QLineEdit();
        if (desc.tag == Properties::Tags::READONLY)
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

        // Drag & drop: accept a compatible Hierarchy GameObject / Folder-view
        // asset dropped onto this row (see RefDropFilter). The read-only line
        // edit would otherwise swallow drops, so let them fall through to the
        // container that hosts the filter.
        m_edit->setAcceptDrops(false);
        m_container->setAcceptDrops(true);
        m_container->installEventFilter(new RefDropFilter(m_desc, [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        }, m_container));
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
        // Game objects have no file path — they keep the grey placeholder.

        auto* picker = new AssetPickerWidget(std::move(items), std::move(thumbGen), std::move(fallbackIconGen), m_container);
        picker->onSelected = [this](const std::string& id) {
            SetValue(id);
            if (onChanged) onChanged(id);
        };
        auto pos = m_container->rect().topLeft();
        pos.setX(pos.x() - picker->width());
        picker->move(m_container->mapToGlobal(pos));
        picker->show();
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
        } else {
            auto* container = Engine::Get()->GetActiveContainer();
            if (!container) return id;
            auto* sm = container->FindSystem<SceneManager>();
            if (!sm) return id;
            for (auto* scene : sm->GetScenes())
                for (auto* go : scene->GetAllGameObjects())
                    if (go->GetID() == id) return go->GetName();
            return id;
        }
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
            " font-size: 16px; padding: 0 2px; }"
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
        auto pos = m_container->rect().topLeft();
        pos.setX(pos.x() - picker->width());
        pos.setY(pos.y() - picker->height()/2);
        picker->move(m_container->mapToGlobal(pos));
        picker->show();
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
