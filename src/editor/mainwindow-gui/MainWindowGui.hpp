#pragma once
#include <QMainWindow>
#include <QSettings>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    static MainWindow* Get() {
        static MainWindow* instance = new MainWindow(nullptr);
        return instance;
    }
    void Init();
    void Start();
    void Shutdown();
    void ClearLayout();
private:
    explicit MainWindow(QWidget* parent = nullptr);
    void LoadLayout();
    void SaveLayout();
    ~MainWindow();


};
