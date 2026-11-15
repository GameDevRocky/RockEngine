#pragma once

#include <QString>
#include <QStringList>

// Authoring operation for files shown by FolderView. The caller supplies only the
// new stem; extensions and paired asset source/descriptor files stay together.
class ProjectFileRenamer {
public:
    static QString EditableName(const QString& path);
    static bool CanRename(const QString& path);
    static bool Rename(const QString& path, const QString& newName, QString* error);
    // Move a selection as one operation, including paired descriptors/sources
    // and known references. Existing destinations are never overwritten.
    static bool CanMove(const QStringList& paths, const QString& directory, QString* error = nullptr);
    static bool Move(const QStringList& paths, const QString& directory, QString* error);
};
