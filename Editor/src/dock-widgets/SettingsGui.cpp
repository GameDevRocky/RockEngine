#include "dock-widgets/SettingsGui.hpp"

#include "Engine.hpp"
#include "engine/core/Globals.hpp"
#include "utils/EditorUtils.hpp"
#include "utils/ProperyFactory.hpp"

#include <QAbstractItemView>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include <cmath>
#include <memory>
#include <string>

namespace {

Globals* ProjectGlobals()
{
    return Globals::Get();
}

QString PropertyToolTip(const GlobalProperty& property, bool editable)
{
    QString text = QString::fromStdString(property.descriptor.description);
    const QString persistence = editable
        ? QStringLiteral("Changes are saved to Globals.config immediately.")
        : QStringLiteral("Stop play mode to edit project globals.");
    if (!text.isEmpty()) text += QStringLiteral("\n\n");
    return text + persistence;
}

template<typename T>
void CommitValue(PropertyWidget<T>* widget,
                 const std::string& groupName,
                 const std::string& propertyName,
                 const T& next)
{
    Globals* globals = ProjectGlobals();
    if (!globals) return;

    const GlobalProperty* property = globals->FindProperty(groupName, propertyName);
    const T* oldValue = property ? std::get_if<T>(&property->value) : nullptr;
    if (!oldValue) return;
    const T previous = *oldValue;

    if (globals->SetValue(groupName, propertyName, GlobalValue(next)) &&
        globals->SaveConfig()) return;

    if (globals->CanEdit())
        globals->SetValue(groupName, propertyName, GlobalValue(previous));
    if (widget->IsValid()) widget->SetValue(previous);
}

template<typename T>
std::unique_ptr<PropertyWidgetBase> CreatePropertyWidget(
    const GlobalProperty& property,
    const std::string& groupName,
    const Properties::PropDesc& descriptor)
{
    auto* widget = PropertyFactory::Create<T>(descriptor);
    widget->SetValue(std::get<T>(property.value));
    const std::string propertyName = property.name;
    widget->onChanged = [widget, groupName, propertyName](T value) {
        CommitValue(widget, groupName, propertyName, value);
    };
    return std::unique_ptr<PropertyWidgetBase>(widget);
}

std::unique_ptr<PropertyWidgetBase> CreateIntegerWidget(
    const GlobalProperty& property,
    const std::string& groupName,
    const Properties::PropDesc& descriptor)
{
    const int initial = std::get<int>(property.value);
    const std::string propertyName = property.name;

    // Inspector integer fields use the numeric float editor with zero decimals;
    // an actual PropertyWidget<int> is its dropdown specialization.
    if (descriptor.tag == Properties::Tags::DROPDOWN) {
        auto* widget = PropertyFactory::Create<int>(descriptor);
        widget->SetValue(initial);
        widget->onChanged = [widget, groupName, propertyName](int value) {
            CommitValue(widget, groupName, propertyName, value);
        };
        return std::unique_ptr<PropertyWidgetBase>(widget);
    }

    auto* widget = PropertyFactory::Create<float>(descriptor);
    widget->SetValue(static_cast<float>(initial));
    widget->onChanged = [widget, groupName, propertyName](float value) {
        Globals* globals = ProjectGlobals();
        if (!globals) return;

        const GlobalProperty* property = globals->FindProperty(groupName, propertyName);
        const int* oldValue = property ? std::get_if<int>(&property->value) : nullptr;
        if (!oldValue) return;
        const int previous = *oldValue;
        const int next = static_cast<int>(std::lround(value));

        if (globals->SetValue(groupName, propertyName, GlobalValue(next)) &&
            globals->SaveConfig()) return;

        if (globals->CanEdit())
            globals->SetValue(groupName, propertyName, GlobalValue(previous));
        if (widget->IsValid()) widget->SetValue(static_cast<float>(previous));
    };
    return std::unique_ptr<PropertyWidgetBase>(widget);
}

std::unique_ptr<PropertyWidgetBase> CreateWidget(const GlobalProperty& property,
                                                 const std::string& groupName,
                                                 bool editable)
{
    Properties::PropDesc descriptor = property.descriptor;
    descriptor.ReadOnly(!editable || descriptor.readOnly);

    switch (property.type) {
        case GlobalValueType::Bool:
            return CreatePropertyWidget<bool>(property, groupName, descriptor);
        case GlobalValueType::Int:
            return CreateIntegerWidget(property, groupName, descriptor);
        case GlobalValueType::Float:
            return CreatePropertyWidget<float>(property, groupName, descriptor);
        case GlobalValueType::String:
            return CreatePropertyWidget<std::string>(property, groupName, descriptor);
        case GlobalValueType::Vector2:
            return CreatePropertyWidget<glm::vec2>(property, groupName, descriptor);
        case GlobalValueType::Vector3:
            return CreatePropertyWidget<glm::vec3>(property, groupName, descriptor);
        case GlobalValueType::Vector4:
        case GlobalValueType::Color:
            return CreatePropertyWidget<glm::vec4>(property, groupName, descriptor);
    }
    return {};
}

} // namespace

SettingsGui* SettingsGui::Get()
{
    static SettingsGui* instance = new SettingsGui(nullptr);
    return instance;
}

SettingsGui::SettingsGui(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(620, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    navigation = new QListWidget(splitter);
    navigation->setMinimumWidth(150);
    navigation->setMaximumWidth(230);
    navigation->setSelectionMode(QAbstractItemView::SingleSelection);
    QFont navigationFont = navigation->font();
    navigationFont.setPointSize(navigationFont.pointSize() + 3);
    navigationFont.setBold(true);
    navigation->setFont(navigationFont);
    navigation->addItem("Globals");
    navigation->setCurrentRow(0);
    splitter->addWidget(navigation);

    auto* scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(scroll);
    pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(16, 14, 16, 16);
    pageLayout->setSpacing(12);
    pageLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(page);
    splitter->addWidget(scroll);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 180, 620 });
    root->addWidget(splitter);

    connect(navigation, &QListWidget::itemSelectionChanged, this, [this]() {
        if (navigation->currentRow() < 0) navigation->setCurrentRow(0);
        RebuildGlobalsPage();
    });
}

SettingsGui::~SettingsGui() = default;

void SettingsGui::Init()
{
    if (initialized) return;
    initialized = true;

    // The Globals instance changes identity on both sides of play mode. Rebuild
    // from the active container and let CanEdit() control read-only state.
    Engine::Get()->Subscribe([this]() {
        Refresh();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);
    Engine::Get()->Subscribe([this]() {
        Refresh();
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);

    Refresh();
}

void SettingsGui::Refresh()
{
    if (pageLayout) RebuildGlobalsPage();
}

void SettingsGui::ClearPage()
{
    // Delete Qt controls first so their signal connections are gone before the
    // non-QObject PropertyWidget wrappers that those connections capture.
    while (QLayoutItem* item = pageLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) delete widget;
        delete item;
    }
    propertyWidgets.clear();
}

void SettingsGui::RebuildGlobalsPage()
{
    ClearPage();

    auto* title = new QLabel("Globals", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 7);
    titleFont.setBold(true);
    title->setFont(titleFont);
    pageLayout->addWidget(title);

    Globals* globals = ProjectGlobals();
    if (!globals) {
        auto* unavailable = new QLabel("Globals system is unavailable.", this);
        unavailable->setEnabled(false);
        pageLayout->addWidget(unavailable);
        pageLayout->addStretch();
        return;
    }

    const bool editable = globals->CanEdit();
    if (!editable) {
        auto* notice = new QLabel(
            "Project globals are read-only while the game is running.", this);
        notice->setWordWrap(true);
        notice->setEnabled(false);
        pageLayout->addWidget(notice);
    }

    for (const GlobalGroup& group : globals->GetGroups()) {
        // A plain section keeps the grouped hierarchy without QGroupBox's
        // enclosing frame, so the global rows sit directly on the settings page.
        auto* section = new QWidget(this);
        auto* grid = new QGridLayout(section);
        grid->setContentsMargins(4, 0, 4, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 2);

        auto* groupTitle = new QLabel(QString::fromStdString(group.name), section);
        QFont groupTitleFont = groupTitle->font();
        groupTitleFont.setPointSize(groupTitleFont.pointSize() + 4);
        groupTitleFont.setBold(true);
        groupTitle->setFont(groupTitleFont);
        grid->addWidget(groupTitle, 0, 0, 1, 2);

        int row = 1;
        for (const GlobalProperty& property : group.properties) {
            std::unique_ptr<PropertyWidgetBase> editor =
                CreateWidget(property, group.name, editable);
            if (!editor) continue;

            QWidget* editorWidget = editor->GetWidget();
            const QString toolTip = PropertyToolTip(property, editable);
            editorWidget->setToolTip(toolTip);

            auto* label = new EditorUtils::ElidingLabel(
                QString::fromStdString(EditorUtils::FormatLabel(property.name) + ": "),
                section);
            QFont labelFont = label->font();
            labelFont.setBold(true);
            label->setFont(labelFont);
            label->setToolTip(toolTip);
            label->setAlignment(editorWidget->property("labelTopAlign").toBool()
                ? (Qt::AlignLeft | Qt::AlignTop)
                : (Qt::AlignLeft | Qt::AlignVCenter));

            grid->addWidget(label, row, 0);
            grid->addWidget(editorWidget, row, 1);
            propertyWidgets.push_back(std::move(editor));
            ++row;
        }
        pageLayout->addWidget(section);
    }

    if (globals->GetGroups().empty()) {
        auto* empty = new QLabel("No global properties were loaded.", this);
        empty->setEnabled(false);
        pageLayout->addWidget(empty);
    }
    pageLayout->addStretch();
}
