#pragma once
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QDir>
#include <QSet>

// Hides raw source files in the folder view when their meta counterpart exists,
// so only one widget is shown per logical asset.
//
// Meta / shown file   Source files hidden alongside it
// ─────────────────   ─────────────────────────────────────────────────────
// foo.png.texture     foo.png / foo.jpg / foo.jpeg / foo.bmp
// foo.vert.shader     foo.vert  AND  foo.frag (same stem, same dir)
// foo.mat             (no source – already a definition file, always shown)
// foo.material        (no source – already a definition file, always shown)
// foo.sprite          (no source – already a definition file, always shown)
class AssetFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit AssetFilterProxyModel(QFileSystemModel* fsModel, QObject* parent = nullptr)
        : QSortFilterProxyModel(parent), m_fsModel(fsModel) {}

    QFileSystemModel* fsModel() const { return m_fsModel; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        QModelIndex idx = m_fsModel->index(sourceRow, 0, sourceParent);
        if (!idx.isValid()) return true;

        QFileInfo fi = m_fsModel->fileInfo(idx);
        if (fi.isDir()) return true;

        const QString ext = fi.suffix().toLower();

        // ── Texture source (.png / .jpg / .jpeg / .bmp) ─────────────────────
        // Hide when foo.ext.texture exists in the same directory
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp") {
            QString metaPath = fi.absoluteFilePath() + ".texture";
            if (QFileInfo::exists(metaPath)) return false;
        }

        // ── Shader source (.glsl) ────────────────────────────────────────────────
        // Hide when foo.glsl.shader exists in the same directory
        if (ext == "glsl") {
            QString metaPath = fi.absoluteFilePath() + ".shader";
            if (QFileInfo::exists(metaPath)) return false;
        }

        return true;
    }

private:
    QFileSystemModel* m_fsModel;
};
