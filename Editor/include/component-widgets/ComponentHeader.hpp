#pragma once
#include <vector>
#include <utility>
#include "utils/CollapsableWidget.hpp"

class Observable;

class ComponentHeader : public CollapsableWidget{
    Q_OBJECT
public:
    explicit ComponentHeader(QWidget* parent = nullptr);
    explicit ComponentHeader(std::string label, QWidget* parent = nullptr);
    void OnActiveToggled(bool val) override;
    virtual void OnLabelEdited(const QString &name){};
    virtual void Bind(std::string id = "");

    // Engine-side subscriptions Bind() registered on the bound Component. The owner
    // must Unsubscribe these on rebuild.
    const std::vector<std::pair<Observable*, int>>& GetSubscriptions() const {
        return m_subs;
    }


protected:
    void paintEvent(QPaintEvent *event);
    std::string component_id = "";
    std::vector<std::pair<Observable*, int>> m_subs;

};