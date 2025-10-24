#pragma once
#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    static MainWindow* Get() {
        static MainWindow* instance = new MainWindow(nullptr);
        return instance;
    }
    void Init();
private:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;


};
