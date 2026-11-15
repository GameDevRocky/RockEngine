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
#include <QPixmap>
#include <QModelIndex>
#include <QRect>
#include <utility>
#include "utils/AssetFilterProxyModel.hpp"

class FolderAssetModel;
class AssetPreviewDelegate;
class QLineEdit;
class QSlider;

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

Q_SIGNALS:
    // A project file with no live Serializable representation should still have
    // something useful to show in the Inspector. MainWindow routes this request
    // there without coupling the Folder view directly to InspectorGui.
    void InspectFileRequested(const QString& filePath);

private:
    explicit FolderViewGui(QWidget* parent = nullptr);
    ~FolderViewGui() override;
    void GoBack();
    void RefreshBreadcrumbs();
    void NavigateToPath(const QString& path, bool recordHistory = true);
    QString RelativePathForBreadcrumb(const QString& path) const;

    // Right-click "New" menu: create folders and asset files on disk.
    void ShowContextMenu(const QPoint& pos);
    void BeginRename(const QModelIndex& index);
    void CommitRename();
    void CancelRename();
    void CreateNewFolder();
    void CreateNewMaterial();
    void CreateNewTileMap();
    void CreateNewTileSet();
    void CreateNewScene();
    void CreateNewScript();
    // A non-colliding path in `folder` for "<baseName><ext>", adding "<sep>1",
    // "<sep>2", … before the extension if needed. Scripts pass an empty separator
    // because the file stem is also a Python module name.
    QString UniquePathIn(const QString& folder, const QString& baseName,
                         const QString& ext, const QString& separator = " ") const;
    // The same, in the folder currently being browsed.
    QString UniqueAssetPath(const QString& baseName, const QString& ext) const;

    // Delete selected file assets and sprite sub-assets after one asynchronous
    // confirmation. Sprite removal rewrites its texture descriptor through
    // AssetManager; file assets also remove their backing files.
    void DeleteSelectedAssets();
    void DeleteOneAsset(const QString& filePath);

    bool eventFilter(QObject* obj, QEvent* event) override;
    FolderAssetModel* m_entries = nullptr;
    AssetPreviewDelegate* m_delegate = nullptr;

    QListView*              gridView          = nullptr;
    QFileSystemModel*       model             = nullptr;
    AssetFilterProxyModel*  proxy             = nullptr;
    QWidget*                navBar            = nullptr;
    QHBoxLayout*            breadcrumbLayout  = nullptr;
    QString                 currentPath;
    QString                 projectDirectory;
    QStack<QString>         directoryHistory;
    QLineEdit*              m_renameEditor = nullptr;
    QSlider*                m_cellSizeSlider = nullptr;
    QString                 m_renamePath;
};
