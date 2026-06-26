#pragma once

#include <QWidget>
#include <string>
#include <vector>
#include <utility>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>
#include "utils/CollapsableWidget.hpp"

class Observable;

class ObjectHeader : public CollapsableWidget
{
    Q_OBJECT

public:
    explicit ObjectHeader(QWidget* parent = nullptr);
    explicit ObjectHeader(std::string label, QWidget* parent = nullptr);
    void Bind(const std::string id) override;
    void OnActiveToggled(bool val) override {};
    void OnLabelEdited(const QString &name) override {};

    // Engine-side subscriptions Bind() registered on the bound GameObject. The
    // owner must Unsubscribe these on rebuild (Qt connects auto-disconnect, these
    // don't).
    const std::vector<std::pair<Observable*, int>>& GetSubscriptions() const {
        return m_subs;
    }

protected:
    void paintEvent(QPaintEvent *event);


private:
    std::string gameobject_id = "";
    QComboBox* tagOptions = nullptr;
    QComboBox* layerOptions= nullptr;
    std::vector<std::pair<Observable*, int>> m_subs;


};