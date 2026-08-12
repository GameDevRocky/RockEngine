#pragma once
#include "utils/PropertyWidget.hpp"
#include <QVBoxLayout>
#include <QToolButton>
#include <vector>

class PropertyFactory {
public:
    template<typename T>
    static PropertyWidget<T>* Create(const Properties::PropDesc& desc);
};


template<>
inline PropertyWidget<float>* PropertyFactory::Create<float>(const Properties::PropDesc& desc) {
    return new FloatPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec2>* PropertyFactory::Create<glm::vec2>(const Properties::PropDesc& desc) {
    return new Vec2PropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec3>* PropertyFactory::Create<glm::vec3>(const Properties::PropDesc& desc) {
    return new Vec3PropertyWidget(desc);
}

template<>
inline PropertyWidget<bool>* PropertyFactory::Create<bool>(const Properties::PropDesc& desc) {
    return new BoolPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec4>* PropertyFactory::Create<glm::vec4>(const Properties::PropDesc& desc) {
    if (desc.tag == Properties::Tags::VECTOR4)
        return new Vec4PropertyWidget(desc);
    return new Vec4ColorPropertyWidget(desc);
}
template<>
inline PropertyWidget<std::string>* PropertyFactory::Create<std::string>(const Properties::PropDesc& desc) {
    if (desc.refType == Properties::Tags::OBJECT_REF) {
        // Texture / sprite / material now use the compact ObjectRefPropertyWidget
        // (line-edit + picker) with a hover thumbnail preview, same as every other
        // ref type — no more always-visible collapsible thumbnail.
        return new ObjectRefPropertyWidget(desc);
    }
    // Paragraph text (a TextRenderer's string, a font's charset) gets the
    // resizable multi-line box; everything else stays a one-line field.
    if (desc.tag == Properties::Tags::MULTILINE)
        return new TextBoxPropertyWidget(desc);
    return new StringPropertyWidget(desc);
}

template<>
inline PropertyWidget<int>* PropertyFactory::Create<int>(const Properties::PropDesc& desc) {
    return new DropdownPropertyWidget(desc);
}

// ─────────────────────────────────────────────────────────────────────────────
// Nested list widget. A PropertyWidget<std::vector<T>> that renders one child
// row per element, recursively built from the element PropDesc via the factory.
// Lives here (not in PropertyWidget.hpp) because it depends on PropertyFactory.
//
// Read-only mode (PropDesc::readOnly): drops the +Add button and the per-row
// −Remove buttons. That's the only difference — rows are otherwise built exactly
// like editable ones.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
class ListPropertyWidget : public PropertyWidget<std::vector<T>> {
public:
    explicit ListPropertyWidget(const Properties::PropDesc& desc)
        : m_elementDesc(desc.elementDesc ? *desc.elementDesc : Properties::PropDesc()),
          m_readOnly(desc.readOnly)
    {
        QWidget* container = new QWidget();
        m_container = container;

        auto* outer = new QVBoxLayout(container);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(2);

        // Collapse toggle header (arrow + element count).
        m_toggle = new QToolButton();
        m_toggle->setCheckable(true);
        m_toggle->setChecked(true);
        m_toggle->setArrowType(Qt::DownArrow);
        m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_toggle->setAutoRaise(true);
        m_toggle->setStyleSheet("QToolButton { border: none; }");
        QObject::connect(m_toggle, &QToolButton::toggled, [this](bool checked) {
            if (!m_toggle.isNull())
                m_toggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            if (!m_body.isNull())
                m_body->setVisible(checked);
        });

        // Header row: collapse toggle on the left, Add button on the right.
        QWidget* header = new QWidget();
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(2);
        headerLayout->addWidget(m_toggle);
        headerLayout->addStretch(1);

        if (!m_readOnly) {
            auto* addButton = new QPushButton("+");
            addButton->setContentsMargins(0,0,0,0);
            addButton->setFixedSize(48, 24);
            addButton->setToolTip("Add");
            QObject::connect(addButton, &QPushButton::clicked, [this]() {
                m_data.push_back(T{});
                rebuild();
                emitChanged();
            });
            headerLayout->addWidget(addButton);
        }
        outer->addWidget(header);

        // Bordered body that holds the element rows (+ Add button). Collapses with
        // the toggle. The ID selector keeps the outline off the child widgets.
        QWidget* body = new QWidget();
        m_body = body;
        body->setObjectName("listBody");
        body->setStyleSheet("QWidget#listBody { border: 1px solid #5a5a5a; border-radius: 3px; }");
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(4, 4, 4, 4);
        bodyLayout->setSpacing(2);
        outer->addWidget(body);

        QWidget* rows = new QWidget();
        m_rows = rows;
        m_rowsLayout = new QVBoxLayout(rows);
        m_rowsLayout->setContentsMargins(0, 0, 0, 0);
        m_rowsLayout->setSpacing(2);
        bodyLayout->addWidget(rows);
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_container.isNull(); }

    void SetValue(const std::vector<T>& vals) override {
        // Guard: a self-originated edit fires the field's change event, which the
        // BindProperty subscription answers with SetValue(getter()) — the same
        // value we just produced. Skip the rebuild to avoid focus loss / churn.
        if (vals == m_data) return;
        m_data = vals;
        rebuild();
    }

    std::vector<T> GetValue() override { return m_data; }

private:
    void emitChanged() {
        if (this->onChanged) this->onChanged(m_data);
    }

    void clearRows() {
        for (auto* child : m_children) delete child;
        m_children.clear();
        if (m_rowsLayout.isNull()) return;
        QLayoutItem* item;
        while ((item = m_rowsLayout->takeAt(0)) != nullptr) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
    }

    void rebuild() {
        clearRows();
        if (!m_toggle.isNull()) {
            int n = static_cast<int>(m_data.size());
            m_toggle->setText(QString("%1 item%2").arg(n).arg(n == 1 ? "" : "s"));
        }
        for (std::size_t i = 0; i < m_data.size(); ++i) {
            PropertyWidget<T>* child = PropertyFactory::Create<T>(m_elementDesc);
            child->SetValue(m_data[i]);

            child->onChanged = [this, i](T val) {
                if (i < m_data.size()) m_data[i] = val;
                emitChanged();
            };

            QWidget* row = new QWidget();
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);
            rl->addWidget(child->GetWidget(), 1);

            // Read-only lists drop the per-row remove button (the +Add button is
            // likewise omitted from the header) — that's all read-only changes.
            if (!m_readOnly) {
                auto* removeBtn = new QPushButton();
                removeBtn->setIcon(removeBtn->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
                removeBtn->setFlat(true);
                removeBtn->setFixedWidth(24);
                removeBtn->setToolTip("Remove");
                QObject::connect(removeBtn, &QPushButton::clicked, [this, i]() {
                    if (i < m_data.size()) {
                        m_data.erase(m_data.begin() + i);
                        rebuild();
                        emitChanged();
                    }
                });
                rl->addWidget(removeBtn);
            }

            m_rowsLayout->addWidget(row);
            m_children.push_back(child);
        }
    }

    Properties::PropDesc m_elementDesc;
    bool m_readOnly = false;
    std::vector<T> m_data;
    std::vector<PropertyWidget<T>*> m_children;
    QPointer<QWidget> m_container;
    QPointer<QToolButton> m_toggle;
    QPointer<QWidget> m_body;
    QPointer<QWidget> m_rows;
    QPointer<QVBoxLayout> m_rowsLayout;
};

template<>
inline PropertyWidget<std::vector<bool>>* PropertyFactory::Create<std::vector<bool>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<bool>(desc);
}

template<>
inline PropertyWidget<std::vector<float>>* PropertyFactory::Create<std::vector<float>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<float>(desc);
}

template<>
inline PropertyWidget<std::vector<std::string>>* PropertyFactory::Create<std::vector<std::string>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<std::string>(desc);
}