#pragma once
#include "utils/PropertyWidget.hpp"
#include "utils/ListRowChrome.hpp"
#include "utils/EditorUtils.hpp"
#include "engine/components/ComponentActions.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "engine/serialization/Registry.hpp"
#include <iostream>
#include <QVBoxLayout>
#include <QToolButton>
#include <QStringList>
#include <QMenu>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

class PropertyFactory {
public:
    template<typename T>
    static PropertyWidget<T>* Create(const Properties::PropDesc& desc);
};

// Defined at the bottom of this header, because a call entry builds an argument
// editor with PropertyFactory itself and so cannot be declared above it. Routed
// to from Create<std::string> the same way ListPropertyWidget is reached.
inline PropertyWidget<std::string>* MakeCallEntryWidget(const Properties::PropDesc& desc);


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
    // One wired call inside an Event field. Checked first: a call entry is
    // carried as a string like every ref, so any later branch would claim it.
    if (desc.tag == Properties::Tags::CALL_ENTRY)
        return MakeCallEntryWidget(desc);
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


// ─────────────────────────────────────────────────────────────────────────────
// CallEntryPropertyWidget — one wired call inside an Event field.
//
// The value is the flat encoded string script_event.py writes:
//     <objectId>|<componentId>|<method>|<argument>
// Two ids rather than one because a GameObject can hold several scripts, so the
// component id is what dispatches, while the object id is what the target picker
// binds to and what still identifies a target that has since been deleted.
//
// Laid out on two lines (target above, method + argument below) for the same
// reason Unity does it: three controls side by side do not fit the inspector's
// value column, let alone inside a list row that also carries a drag grip and a
// remove button.
// ─────────────────────────────────────────────────────────────────────────────
class CallEntryPropertyWidget : public PropertyWidget<std::string> {
public:
    explicit CallEntryPropertyWidget(const Properties::PropDesc& desc)
        : m_readOnly(desc.readOnly)
    {
        // One line: target, the call with its argument, and the invoke button.
        // The Event field asks for a full inspector row (PropDesc::FullRow),
        // which is what makes three controls abreast fit at all.
        m_container = new QWidget();
        m_rowLayout = new QHBoxLayout(m_container);
        m_rowLayout->setContentsMargins(0, 0, 0, 0);
        m_rowLayout->setSpacing(4);

        // Target: any GameObject. Unfiltered on purpose — what can be called is
        // decided by the method list, not by narrowing the picker, so a target
        // with no actions is visibly a target with no actions.
        m_target = new ObjectRefPropertyWidget(
            Properties::PropDesc().Tag(Properties::Tags::OBJECT_REF)
                                  .RefType(Properties::Tags::OBJECT_REF)
                                  .ReadOnly(m_readOnly));
        m_target->onChanged = [this](const std::string& id) {
            if (id == m_objectId) return;
            // A new target invalidates the method and its argument: the old
            // method belonged to a component this object does not have.
            m_objectId = id;
            m_componentId.clear();
            m_method.clear();
            m_rawArg.clear();
            refreshCall();
            emitChanged();
        };
        m_rowLayout->addWidget(m_target->GetWidget(), 3);

        // The chosen call, shown the way it reads in code and doubling as the
        // picker: clicking anywhere but the argument opens the method menu. That
        // menu is built fresh on every open, which is also what keeps it correct
        // — the target's script can gain an @action from a hot-reload at any
        // moment, and this widget is not rebuilt when a DIFFERENT object's script
        // is the one that changed.
        m_call = new FunctionCallWidget();
        m_call->onActivated = [this]() { openMethodMenu(); };
        m_call->SetInvocable(!m_readOnly,
            m_readOnly ? QStringLiteral("Read-only.") : QStringLiteral("Choose a function"));
        m_rowLayout->addWidget(m_call, 4);

        // Fires this one wired call with whatever its argument currently holds.
        // Per-row rather than a button per action further down the inspector: the
        // thing you want to test is a specific call, and it is already spelled out
        // right here with the argument you set.
        m_invoke = new QPushButton();
        m_invoke->setObjectName("InvokeButton");
        m_invoke->setToolTip("Invoke Method");
        m_invoke->setIcon(QIcon(QString::fromStdString(
            EngineUtils::GetAssetPath("Domain/lib/assets/icons/invoke_icon.png"))));
        m_invoke->setIconSize(QSize(14, 14));
        m_invoke->setFixedSize(24, 24);
        QObject::connect(m_invoke, &QPushButton::clicked, [this]() { invokeNow(); });
        m_rowLayout->addWidget(m_invoke, 0);
        refreshInvokeState();
    }

    QWidget* GetWidget() override { return m_container; }
    bool IsValid() override { return !m_container.isNull(); }

    void SetValue(const std::string& encoded) override {
        if (encoded == Encode()) return;   // our own edit echoing back
        Decode(encoded);
        if (m_target) m_target->SetValue(m_objectId);
        refreshCall();
    }

    std::string GetValue() override { return Encode(); }

private:
    // Mirrors Domain/lib/api/core/script_event.py. The argument is last and is
    // arbitrary user text, so it is the only field allowed to contain the
    // separator — hence splitting a bounded number of times rather than all.
    static constexpr char kSep = '|';

    // Fire this row's call now. Play mode only, matching the rule that an action
    // is game logic: running one against the edit-time world would mutate the
    // scene with nothing on the undo stack to take it back. Resolves through the
    // active registry so the runtime copy of the component is the one that runs.
    void invokeNow() {
        if (m_componentId.empty() || m_method.empty()) return;
        Component* target = Registry::FindInRuntime<Component>(m_componentId);
        if (!target) {
            std::cerr << "[Event] target component " << m_componentId
                      << " is not in the running world\n";
            return;
        }
        std::string error;
        if (!ComponentActions::Invoke(target, m_method, m_rawArg, &error))
            std::cerr << "[Event] " << m_method << ": " << error << std::endl;
    }

    // The button is live only while playing, and only once a call is actually
    // wired — an empty row has nothing to fire.
    void refreshInvokeState() {
        if (m_invoke.isNull()) return;
        Container* active = Engine::Get()->GetActiveContainer();
        const bool playing = active && active->GetMode() == Container::Mode::Runtime;
        const bool wired = !m_componentId.empty() && !m_method.empty();
        m_invoke->setEnabled(playing && wired && !m_readOnly);
        m_invoke->setToolTip(!wired   ? QStringLiteral("Pick a target and a function first.")
                           : !playing ? QStringLiteral("Runs in Play mode. The argument can be set up now.")
                                      : QStringLiteral("Invoke this call now"));
    }

    void Decode(const std::string& s) {
        std::string parts[4];
        std::size_t start = 0;
        for (int i = 0; i < 3; ++i) {
            const std::size_t hit = s.find(kSep, start);
            if (hit == std::string::npos) { parts[i] = s.substr(start); start = s.size(); continue; }
            parts[i] = s.substr(start, hit - start);
            start = hit + 1;
        }
        parts[3] = start <= s.size() ? s.substr(start) : std::string();
        m_objectId = parts[0]; m_componentId = parts[1];
        m_method   = parts[2]; m_rawArg      = parts[3];
    }

    std::string Encode() const {
        return m_objectId + kSep + m_componentId + kSep + m_method + kSep + m_rawArg;
    }

    void emitChanged() { if (onChanged) onChanged(Encode()); }

    GameObject* targetObject() const {
        if (m_objectId.empty()) return nullptr;
        Container* container = Engine::Get()->GetActiveContainer();
        SceneManager* scenes = container ? container->FindSystem<SceneManager>() : nullptr;
        if (!scenes) return nullptr;
        for (Scene* scene : scenes->GetScenes())
            for (GameObject* go : scene->GetAllGameObjects())
                if (go->GetID() == m_objectId) return go;
        return nullptr;
    }

    // Every action on every component of the target, keyed "<componentId>/<name>".
    // Rebuilt on demand rather than cached: ComponentActions::For asks a
    // ScriptComponent for its live @action list, which changes on every
    // hot-reload, so anything held across time goes stale.
    std::map<QString, ComponentActionInfo> collectActions() const {
        std::map<QString, ComponentActionInfo> found;
        GameObject* owner = targetObject();
        if (!owner) return found;
        for (Component* component : owner->GetAllComponents()) {
            if (!component) continue;
            for (const ComponentActionInfo& action : ComponentActions::For(component)) {
                found[QString::fromStdString(component->GetID()) + "/" +
                      QString::fromStdString(action.name)] = action;
            }
        }
        return found;
    }

    // The picker: one submenu per component, reading like Unity's grouped list
    // ("AudioSource ▸ Play", "Enemy ▸ Deal Damage"). Built at the moment it opens,
    // so an action added by a hot-reload seconds ago is already in it.
    void openMethodMenu() {
        if (m_readOnly || m_call.isNull()) return;

        QMenu menu;
        QAction* none = menu.addAction("(no function)");
        QObject::connect(none, &QAction::triggered, [this]() { chooseMethod(QString()); });

        GameObject* owner = targetObject();
        if (!owner) {
            menu.addSeparator();
            menu.addAction("Pick a target object first")->setEnabled(false);
        } else {
            bool any = false;
            for (Component* component : owner->GetAllComponents()) {
                if (!component) continue;
                const auto actions = ComponentActions::For(component);
                if (actions.empty()) continue;
                any = true;
                QMenu* group = menu.addMenu(QString::fromStdString(component->GetTypeName()));
                for (const ComponentActionInfo& action : actions) {
                    const QString key = QString::fromStdString(component->GetID()) + "/" +
                                        QString::fromStdString(action.name);
                    QAction* item = group->addAction(QString::fromStdString(action.label));
                    if (!action.tooltip.empty())
                        item->setToolTip(QString::fromStdString(action.tooltip));
                    QObject::connect(item, &QAction::triggered, [this, key]() { chooseMethod(key); });
                }
            }
            if (!any) {
                menu.addSeparator();
                menu.addAction("This object exposes no actions")->setEnabled(false);
            }
        }
        menu.exec(m_call->mapToGlobal(m_call->rect().bottomLeft()));
    }

    void chooseMethod(const QString& key) {
        const QStringList parts = key.split('/');
        m_componentId = parts.size() == 2 ? parts[0].toStdString() : std::string();
        m_method      = parts.size() == 2 ? parts[1].toStdString() : std::string();
        m_rawArg.clear();      // the new method's argument is a different thing
        refreshCall();
        emitChanged();
    }

    // Repaint the call for whatever is currently selected: its name, and an
    // argument editor of the type that method declares.
    void refreshCall() {
        if (m_call.isNull()) return;
        m_actions = collectActions();

        const QString key = QString::fromStdString(m_componentId) + "/" +
                            QString::fromStdString(m_method);
        auto found = m_actions.find(key);

        if (m_method.empty()) {
            m_call->SetName("No Function");
        } else if (found == m_actions.end()) {
            // A method the target no longer offers keeps its slot rather than
            // silently resetting — the wiring is still what the scene says, and
            // dropping it just because the object was selected would destroy data
            // by looking at it.
            m_call->SetName(QString("Missing \"%1\"").arg(QString::fromStdString(m_method)));
        } else {
            m_call->SetName(QString::fromStdString(found->second.label));
        }

        rebuildArgEditor();
        refreshInvokeState();
    }

    // Rebuilt from scratch whenever the method changes, because the argument's
    // very TYPE comes from the chosen method's annotation — a float argument is a
    // number box, a str a text field, a Sprite an asset picker. There is no one
    // widget that could simply be reconfigured.
    void rebuildArgEditor() {
        if (m_call.isNull()) return;
        m_argGet = nullptr;
        m_argWidget = nullptr;

        const QString key = QString::fromStdString(m_componentId) + "/" +
                            QString::fromStdString(m_method);
        auto found = m_actions.find(key);
        if (found == m_actions.end() || !found->second.hasArg) {
            m_call->SetArgWidget(nullptr);   // empty parentheses
            return;
        }

        const ComponentActionArg arg = found->second.arg;
        Properties::PropDesc desc;
        desc.readOnly = m_readOnly;
        desc.step = (arg.typeName == "int") ? 1.0f : 0.1f;

        auto publish = [this]() {
            m_rawArg = m_argGet ? m_argGet() : std::string();
            emitChanged();
        };

        if (arg.typeName == "bool") {
            auto* w = PropertyFactory::Create<bool>(desc.Tag(Properties::Tags::TOGGLE));
            w->SetValue(m_rawArg == "true" || m_rawArg == "1");
            m_argGet = [w]() { return w->GetValue() ? std::string("true") : std::string("false"); };
            w->onChanged = [publish](bool) { publish(); };
            m_argWidget = w->GetWidget();
        } else if (arg.typeName == "float" || arg.typeName == "int") {
            auto* w = PropertyFactory::Create<float>(desc.Tag(Properties::Tags::FLOAT));
            w->SetValue(m_rawArg.empty() ? 0.0f : std::strtof(m_rawArg.c_str(), nullptr));
            const bool isInt = (arg.typeName == "int");
            m_argGet = [w, isInt]() {
                return isInt ? std::to_string(static_cast<int>(w->GetValue()))
                             : QString::number(w->GetValue()).toStdString();
            };
            w->onChanged = [publish](float) { publish(); };
            m_argWidget = w->GetWidget();
        } else if (arg.typeName == "vec2" || arg.typeName == "vec3" || arg.typeName == "vec4") {
            m_argWidget = MakeVecArg(arg.typeName, desc, publish);
        } else {
            // "str", plain or a reference. The ref flavours reuse the very same
            // descriptor shape script FIELDS build, so an argument picker is the
            // identical control as the equivalent field.
            Properties::PropDesc strDesc = desc;
            if (arg.refTypeName == "sprite")
                strDesc.Tag(Properties::Tags::SPRITE).RefType(Properties::Tags::OBJECT_REF);
            else if (arg.refTypeName == "material")
                strDesc.Tag(Properties::Tags::MATERIAL).RefType(Properties::Tags::OBJECT_REF);
            else if (arg.refTypeName.rfind("gameobject:", 0) == 0)
                strDesc.Tag(Properties::Tags::OBJECT_REF).RefType(Properties::Tags::OBJECT_REF)
                       .RefClass(arg.refTypeName.substr(std::string("gameobject:").size()));
            else if (arg.refTypeName.rfind("component:", 0) == 0)
                strDesc.Tag(Properties::Tags::OBJECT_REF).RefType(Properties::Tags::OBJECT_REF)
                       .ComponentType(arg.refTypeName.substr(std::string("component:").size()));
            else
                strDesc.Tag(Properties::Tags::STRING);

            auto* w = PropertyFactory::Create<std::string>(strDesc);
            w->SetValue(m_rawArg);
            m_argGet = [w]() { return w->GetValue(); };
            w->onChanged = [publish](const std::string&) { publish(); };
            m_argWidget = w->GetWidget();
        }

        // Handed to the call widget so it lands between the parentheses.
        m_call->SetArgWidget(m_argWidget);
        if (m_argWidget) m_argWidget->setToolTip(QString::fromStdString(arg.name));
    }

    QWidget* MakeVecArg(const std::string& typeName, Properties::PropDesc desc,
                        const std::function<void()>& publish) {
        // "1,2,3" -> components, tolerating a short or empty string by zero-filling
        // rather than rejecting it: a half-typed argument is an ordinary state.
        auto components = [](const std::string& raw, int count) {
            std::vector<float> out(static_cast<std::size_t>(count), 0.0f);
            std::size_t start = 0;
            for (int i = 0; i < count && start <= raw.size(); ++i) {
                const std::size_t hit = raw.find(',', start);
                out[static_cast<std::size_t>(i)] =
                    std::strtof(raw.substr(start, hit - start).c_str(), nullptr);
                if (hit == std::string::npos) break;
                start = hit + 1;
            }
            return out;
        };
        auto join = [](std::initializer_list<float> vals) {
            QStringList parts;
            for (float v : vals) parts << QString::number(v);
            return parts.join(QChar(',')).toStdString();
        };

        if (typeName == "vec2") {
            auto* w = PropertyFactory::Create<glm::vec2>(desc.Tag(Properties::Tags::VECTOR2));
            const auto c = components(m_rawArg, 2);
            w->SetValue({ c[0], c[1] });
            m_argGet = [w, join]() { auto v = w->GetValue(); return join({ v.x, v.y }); };
            w->onChanged = [publish](glm::vec2) { publish(); };
            return w->GetWidget();
        }
        if (typeName == "vec3") {
            auto* w = PropertyFactory::Create<glm::vec3>(desc.Tag(Properties::Tags::VECTOR3));
            const auto c = components(m_rawArg, 3);
            w->SetValue({ c[0], c[1], c[2] });
            m_argGet = [w, join]() { auto v = w->GetValue(); return join({ v.x, v.y, v.z }); };
            w->onChanged = [publish](glm::vec3) { publish(); };
            return w->GetWidget();
        }
        auto* w = PropertyFactory::Create<glm::vec4>(desc.Tag(Properties::Tags::VECTOR4));
        const auto c = components(m_rawArg, 4);
        w->SetValue({ c[0], c[1], c[2], c[3] });
        m_argGet = [w, join]() { auto v = w->GetValue(); return join({ v.x, v.y, v.z, v.w }); };
        w->onChanged = [publish](glm::vec4) { publish(); };
        return w->GetWidget();
    }

    bool m_readOnly = false;
    std::string m_objectId, m_componentId, m_method, m_rawArg;
    std::map<QString, ComponentActionInfo> m_actions;
    std::function<std::string()> m_argGet;

    QPointer<QWidget> m_container;
    ObjectRefPropertyWidget* m_target = nullptr;
    QPointer<FunctionCallWidget> m_call;
    QPointer<QPushButton> m_invoke;
    QPointer<QHBoxLayout> m_rowLayout;
    QWidget* m_argWidget = nullptr;
};

inline PropertyWidget<std::string>* MakeCallEntryWidget(const Properties::PropDesc& desc) {
    return new CallEntryPropertyWidget(desc);
}
