#pragma once
#include <QWidget>
#include <QListView>
#include <QFileSystemModel>
#include <QPushButton>
#include <QString>
#include <QStack>

class FolderViewGui : public QWidget {
    Q_OBJECT

public:
    static FolderViewGui* Get() {
        static FolderViewGui* instance = nullptr;
        if (!instance) {
            instance = new FolderViewGui(nullptr);
        }
        return instance;
    }
    void Init();
    void Navigate(const std::string& filepath);
    void SetProjectDirectory(const std::string& projectDir);

private:
    explicit FolderViewGui(QWidget* parent = nullptr);
    ~FolderViewGui() override = default;
    void GoBack();
    void UpdateBackButtonState();

    QListView* gridView = nullptr;
    QFileSystemModel* model = nullptr;
    QPushButton* backButton = nullptr;
    QString currentPath;
    QString projectDirectory;
    QStack<QString> directoryHistory;
};