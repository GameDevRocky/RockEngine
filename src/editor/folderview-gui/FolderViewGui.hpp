#pragma once
#include <QWidget>
#include <QListView>
#include <QFileSystemModel>
#include <QString>

class FolderViewGui : public QWidget {
    Q_OBJECT

public:
    static FolderViewGui* Get() {
        static FolderViewGui* instance = new FolderViewGui(nullptr);
        return instance;
    }
    void Init();
    void Navigate(const std::string& filepath);

private:
    explicit FolderViewGui(QWidget* parent = nullptr);
    ~FolderViewGui() override = default;

    QListView* gridView = nullptr;
    QFileSystemModel* model = nullptr;
    QString currentPath;
};