#pragma once

#include <QString>

// Authoring operation for files shown by FolderView. The caller supplies only the
// new stem; extensions and paired asset source/descriptor files stay together.
class ProjectFileRenamer {
public:
    static QString EditableName(const QString& path);
    static bool CanRename(const QString& path);
    static bool Rename(const QString& path, const QString& newName, QString* error);
};
