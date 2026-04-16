#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QToolButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>



class CollapsableWidget : public QWidget {
    Q_OBJECT 
    
public: 
    explicit CollapsableWidget(std::string label, QWidget* parent = nullptr);
    void AddWidget(QWidget* widget);
    void SetIcon(QIcon& icon);
    virtual void OnActiveToggled(bool val){};
    virtual void OnLabelEdited(const QString &name){};
    virtual void Bind(std::string id = ""){};
protected:
    void paintEvent(QPaintEvent *event);

protected:
    QWidget* header = nullptr;
    QLineEdit* label = nullptr;
    QToolButton* toggleButton;
    QCheckBox* activeButton;
    QPushButton* iconButton;
    QWidget* contentWidget;
    QVBoxLayout* mainLayout;
    QVBoxLayout* contentLayout;
};