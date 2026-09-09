#pragma once

#include <memory>
#include <vector>

#include <QWidget>

class QListWidget;
class PropertyWidgetBase;
class QVBoxLayout;

// Project settings surface. Its left navigation contains Globals; the right
// side is generated from Globals.config using the Inspector property widgets.
class SettingsGui : public QWidget {
public:
    static SettingsGui* Get();

    void Init();
    void Refresh();

private:
    explicit SettingsGui(QWidget* parent = nullptr);
    ~SettingsGui() override;

    void RebuildGlobalsPage();
    void ClearPage();

    bool initialized = false;
    QListWidget* navigation = nullptr;
    QVBoxLayout* pageLayout = nullptr;
    std::vector<std::unique_ptr<PropertyWidgetBase>> propertyWidgets;
};
