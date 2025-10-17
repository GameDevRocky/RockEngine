#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QCloseEvent>

class EditorWindow : public QMainWindow {
    Q_OBJECT

public: 
    static EditorWindow& Get() {
        static EditorWindow instance;
        return instance;
    }

    void Init();
    void SetupMenuBar();
    void SetupToolBar();
    void SetupStatusBar();
    void SetupDocks();

    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;
protected:
    void closeEvent(QCloseEvent* event) override; 

private:
    explicit EditorWindow(QWidget* parent = nullptr);
    ~EditorWindow() = default;
    void SaveLayout();
    void LoadLayout();
};
