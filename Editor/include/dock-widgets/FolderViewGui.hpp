#pragma once
#include <QWidget>
#include <QListView>
#include <QFileSystemModel>
#include <QPushButton>
#include <QString>
#include <QStack>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include "utils/AssetFilterProxyModel.hpp"

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
    void RefreshBreadcrumbs();
    void NavigateToPath(const QString& path, bool recordHistory = true);
    QString RelativePathForBreadcrumb(const QString& path) const;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    QListView*              gridView          = nullptr;
    QFileSystemModel*       model             = nullptr;
    AssetFilterProxyModel*  proxy             = nullptr;
    QWidget*                navBar            = nullptr;
    QHBoxLayout*            breadcrumbLayout  = nullptr;
    QString                 currentPath;
    QString                 projectDirectory;
    QStack<QString>         directoryHistory;
};