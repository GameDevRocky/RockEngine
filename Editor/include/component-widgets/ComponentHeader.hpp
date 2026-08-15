#pragma once
#include <vector>
#include <utility>
#include <QPoint>
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
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent *event);
    // Right-click → "Delete Component" menu (disabled for the mandatory Transform).
    void contextMenuEvent(QContextMenuEvent* event) override;
    std::string component_id = "";
    std::vector<std::pair<Observable*, int>> m_subs;
    QPoint m_dragStart;
    bool m_dragArmed = false;

};
