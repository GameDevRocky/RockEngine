#pragma once
#include <vector>
#include <string>
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
    std::vector<std::string> GetSelectedFilePaths() const;

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

    // Right-click "New" menu: create asset files on disk in the current folder.
    void ShowContextMenu(const QPoint& pos);
    void CreateNewMaterial();
    void CreateNewScene();
    // A non-colliding path in the current folder for "<baseName><ext>", adding
    // " 1", " 2", … before the extension if needed.
    QString UniqueAssetPath(const QString& baseName, const QString& ext) const;

    // Delete the currently selected asset(s) after a confirmation prompt. Removes
    // the file from disk (plus any companion source/meta) and the asset from
    // AssetManager memory. Triggered by the Del key or the context-menu action.
    void DeleteSelectedAssets();
    void DeleteOneAsset(const QString& filePath);

    QListView*              gridView          = nullptr;
    QFileSystemModel*       model             = nullptr;
    AssetFilterProxyModel*  proxy             = nullptr;
    QWidget*                navBar            = nullptr;
    QHBoxLayout*            breadcrumbLayout  = nullptr;
    QString                 currentPath;
    QString                 projectDirectory;
    QStack<QString>         directoryHistory;
};