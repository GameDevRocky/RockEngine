#pragma once

#include <QWidget>
#include <string>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include "utils/CollapsableWidget.hpp"
class ObjectHeader : public CollapsableWidget
{
    Q_OBJECT

public:
    explicit ObjectHeader(QWidget* parent = nullptr);
    explicit ObjectHeader(std::string label, QWidget* parent = nullptr);
    void Bind(const std::string id) override;
    void OnActiveToggled(bool val) override {};
    void OnLabelEdited(const QString &name) override {};

protected:
    void paintEvent(QPaintEvent *event);
    

private:
    std::string gameobject_id = "";
    QComboBox* tagOptions = nullptr;
    QComboBox* layerOptions= nullptr;


};