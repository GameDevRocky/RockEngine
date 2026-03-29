#pragma once
#include <QWidget>

class QPushButton;

class RuntimeBar : public QWidget {
    Q_OBJECT

public:
    static RuntimeBar* Get() {
        static RuntimeBar* instance = nullptr;
        if (!instance) {
            instance = new RuntimeBar(nullptr);
        }
        return instance;
    }
    void Init();
    explicit RuntimeBar(QWidget* parent = nullptr);
private:
    ~RuntimeBar() override = default;
    QPushButton* runtimeButton = nullptr;
    QPushButton* pauseButton = nullptr;

    QIcon* playIcon = nullptr;
    QIcon* stopIcon = nullptr;


};

