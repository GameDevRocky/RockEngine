#pragma once
#include "utils/PropertyWidget.hpp"
#include "utils/ListRowChrome.hpp"
#include "utils/EditorUtils.hpp"
#include <QVBoxLayout>
#include <QToolButton>
#include <algorithm>
#include <vector>

class PropertyFactory {
public:
    template<typename T>
    static PropertyWidget<T>* Create(const Properties::PropDesc& desc);
};


template<>
inline PropertyWidget<float>* PropertyFactory::Create<float>(const Properties::PropDesc& desc) {
    // Tags::SLIDER had no widget behind it until now -- it fell through to the
    // spin box, so the four call sites that asked for a slider silently got a
    // number. The bounded-range check is the feature's rule, not a safety net:
    // an unbounded slider has no span to drag along, so it degrades to the
    // ordinary editor rather than rendering a handle that cannot move.
    if (desc.tag == Properties::Tags::SLIDER && HasFiniteRange(desc))
        return new SliderPropertyWidget(desc);
    return new FloatPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec2>* PropertyFactory::Create<glm::vec2>(const Properties::PropDesc& desc) {
    // Same rule as the float slider above; without bounds this is just a pair of
    // numbers, which is exactly what Vec2PropertyWidget already shows.
    if (desc.tag == Properties::Tags::RANGE_SLIDER && HasFiniteRange(desc))
        return new RangeSliderPropertyWidget(desc);
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
    // Reflect[str, Options(...)] on a list element: the row's value is the label
    // itself, so it needs the string-valued combo box rather than the int one.
    if (desc.tag == Properties::Tags::DROPDOWN)
        return new StringDropdownPropertyWidget(desc);
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
        m_toggle->setObjectName("CollapseToggleButton");
        m_toggle->setCheckable(true);
        m_toggle->setChecked(true);
        m_toggle->setArrowType(Qt::NoArrow);
        m_toggle->setIconSize(QSize(24, 24));
        m_toggle->setIcon(EditorUtils::CustomIconProvider::disclosureArrowDownIcon());
        m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_toggle->setAutoRaise(true);
        m_toggle->setStyleSheet("QToolButton { border: none; }");
        QObject::connect(m_toggle, &QToolButton::toggled, [this](bool checked) {
            if (!m_toggle.isNull())
                m_toggle->setIcon(checked
                    ? EditorUtils::CustomIconProvider::disclosureArrowDownIcon()
                    : EditorUtils::CustomIconProvider::disclosureArrowRightIcon());
            if (!m_body.isNull())
                m_body->setVisible(checked);
        });

        // Header row: collapse toggle on the left, Add button on the right.
        QWidget* header = new QWidget();
        auto* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(0,4,4,4);
        headerLayout->setSpacing(2);
        headerLayout->addWidget(m_toggle);
        headerLayout->addStretch(1);

        if (!m_readOnly) {
            auto* addButton = new QPushButton();
            addButton->setIcon(QIcon("Domain/lib/assets/icons/plus_icon.png"));
            addButton->setContentsMargins(0,0,0,0);
            addButton->setFixedSize(24, 24);
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

    // Move an element and republish. The whole backend of reordering: the row
    // widgets are a pure function of m_data, so shifting the data and rebuilding
    // is both the move and the redraw.
    void moveElement(int from, int to) {
        const int n = static_cast<int>(m_data.size());
        if (from < 0 || from >= n) return;
        to = std::clamp(to, 0, n - 1);
        if (from == to) return;

        T moved = std::move(m_data[from]);
        m_data.erase(m_data.begin() + from);
        m_data.insert(m_data.begin() + to, std::move(moved));

        // Queued: this runs from inside the drag handle's own event filter, and
        // rebuild() deletes that handle. Tearing it down underneath the event
        // that asked for it is a use-after-free.
        QMetaObject::invokeMethod(m_container, [this]() {
            rebuild();
            emitChanged();
        }, Qt::QueuedConnection);
    }

    void clearRows() {
        for (auto* child : m_children) delete child;
        m_children.clear();
        if (m_reorder) m_reorder->Clear();
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

            // The grip goes FIRST, before the element's own widget: it is the
            // handle for the row as a whole, so it reads as belonging to the row
            // rather than to the field inside it.
            ListRow::DragHandle* handle = nullptr;
            if (!m_readOnly) {
                handle = new ListRow::DragHandle(row);
                rl->addWidget(handle);
            }

            rl->addWidget(child->GetWidget(), 1);

            // Read-only lists drop the grip and the remove button (the +Add
            // button is likewise omitted from the header) — that's all read-only
            // changes.
            if (!m_readOnly) {
                auto* removeBtn = ListRow::MakeRemoveButton(row);
                QObject::connect(removeBtn, &QPushButton::clicked, [this, i]() {
                    if (i >= m_data.size()) return;
                    m_data.erase(m_data.begin() + i);
                    // Queued for the same reason as moveElement: rebuild() deletes
                    // the button whose click we are inside.
                    QMetaObject::invokeMethod(m_container, [this]() {
                        rebuild();
                        emitChanged();
                    }, Qt::QueuedConnection);
                });
                rl->addWidget(removeBtn);
            }

            m_rowsLayout->addWidget(row);
            if (handle) {
                if (!m_reorder) {
                    m_reorder = new ListRow::ReorderController(
                        m_rows, [this](int from, int to) { moveElement(from, to); },
                        m_container);
                }
                m_reorder->RegisterRow(row, handle);
            }
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
    // Created on the first rebuild that produces rows; parented to m_container so
    // it outlives individual rebuilds but dies with the widget.
    ListRow::ReorderController* m_reorder = nullptr;
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

// Int rows go through Create<int>, which is always the dropdown -- there is no
// plain int editor (a lone int field is edited as a float and converted at the
// boundary). So this is for list[Reflect[int, Options(...)]] and nothing else;
// an int list without options binds as std::vector<float> instead.
template<>
inline PropertyWidget<std::vector<int>>* PropertyFactory::Create<std::vector<int>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<int>(desc);
}

template<>
inline PropertyWidget<std::vector<glm::vec2>>* PropertyFactory::Create<std::vector<glm::vec2>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<glm::vec2>(desc);
}

template<>
inline PropertyWidget<std::vector<glm::vec3>>* PropertyFactory::Create<std::vector<glm::vec3>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<glm::vec3>(desc);
}

template<>
inline PropertyWidget<std::vector<glm::vec4>>* PropertyFactory::Create<std::vector<glm::vec4>>(const Properties::PropDesc& desc) {
    return new ListPropertyWidget<glm::vec4>(desc);
}
