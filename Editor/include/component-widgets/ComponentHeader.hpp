#pragma once
#include "utils/CollapsableWidget.hpp"

class ComponentHeader : public CollapsableWidget{
    Q_OBJECT
public:
    explicit ComponentHeader(QWidget* parent = nullptr);
    explicit ComponentHeader(std::string label, QWidget* parent = nullptr);
    virtual void OnActiveToggled(bool val){};
    virtual void OnLabelEdited(const QString &name){};
    virtual void Bind(std::string id = "");


protected:
    void paintEvent(QPaintEvent *event);
    std::string component_id = "";

};