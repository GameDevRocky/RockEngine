#pragma once

#include <QWidget>
#include <string>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>

class ObjectHeader : public QWidget
{
    Q_OBJECT

public:
    explicit ObjectHeader(QWidget* parent = nullptr);
    void Bind(const std::string& id);
protected:
    void paintEvent(QPaintEvent *event);
    

private:
    std::string gameobject_id = "";
    QRadioButton* activeButton = nullptr;
    QLineEdit* nameEdit = nullptr;
    QComboBox* tagOptions = nullptr;
    QComboBox* layerOptions= nullptr;


};