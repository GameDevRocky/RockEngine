#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QToolButton>
#include <QRadioButton>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

class CollapsableWidget : public QWidget {
    Q_OBJECT 

public: 
    explicit CollapsableWidget(std::string label, QWidget* parent = nullptr);
    void addWidget(QWidget* widget);
    void setIcon(QIcon& icon);
    virtual void OnActiveToggled(bool val){};
    virtual void OnLabelEdited(const QString &name){};
    virtual void Bind(std::string id = ""){};
protected:
    void paintEvent(QPaintEvent *event);

protected:
    QWidget* header = nullptr;
    QLineEdit* label = nullptr;
    QToolButton* toggleButton;
    QRadioButton* activeButton;
    QPushButton* iconButton;
    QWidget* contentWidget;
    QVBoxLayout* mainLayout;
    QVBoxLayout* contentLayout;
};