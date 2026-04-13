#pragma once
#include "utils/CollapsableWidget.hpp"

class ComponentHeader : public CollapsableWidget{
    Q_OBJECT
public:
    explicit ComponentHeader(QWidget* parent = nullptr);
    explicit ComponentHeader(std::string label, QWidget* parent = nullptr);
    void OnActiveToggled(bool val) override;
    virtual void OnLabelEdited(const QString &name){};
    virtual void Bind(std::string id = "");


protected:
    void paintEvent(QPaintEvent *event);
    std::string component_id = "";

};