#pragma once
#include <QWidget>

class ConsoleGui : public QWidget {
    Q_OBJECT

public:
    static ConsoleGui& Get() {
        static ConsoleGui instance(nullptr);
        return instance;
    }

private:
    explicit ConsoleGui(QWidget* parent = nullptr);
    ~ConsoleGui() override = default;
};
