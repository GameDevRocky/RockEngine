#include "EditorUtils.hpp"
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <vector>

namespace EditorUtils {

QIcon CustomIconProvider::icon(const QFileInfo &info) const {
    if (info.isDir()) {
        return QIcon("domain/assets/icons/folder_icon.png");
    } else if (info.suffix() == "yaml") {
        return QIcon("domain/assets/icons/yaml_icon.png");
    } else if (info.suffix() == "cpp") {
        return QIcon("domain/assets/icons/cpp_icon.png");
    } else if (info.suffix() == "hpp") {
        return QIcon("domain/assets/icons/cpp_icon.png");
    } else if (info.suffix() == "h") {
        return QIcon("domain/assets/icons/header_icon.png");
    } else if (info.suffix() == "png" || info.suffix() == "jpg" || info.suffix() == "jpeg") {
        // Load the actual image file as a thumbnail
        return QIcon(info.absoluteFilePath());
    }
    // Fallback to default file icon
    return QFileIconProvider::icon(info);
}

void OpenInVSCode(const std::string& fullPath)
{
    std::string pathOnly = fullPath;
    int lineNumber = -1;
    size_t colon = fullPath.rfind(':'); 
    if (colon != std::string::npos && colon > 1) {
        try {
            lineNumber = std::stoi(fullPath.substr(colon + 1));
            pathOnly = fullPath.substr(0, colon);
        } catch (...) {
            lineNumber = -1;
        }
    }

    QString qPath = QString::fromStdString(pathOnly);

    QStringList args;
    if (lineNumber > 0) {
        args << "--goto" << QString("%1:%2").arg(qPath).arg(lineNumber);
    } else {
        args << qPath;
    }

    // Full path to VS Code exe (adjust if installed somewhere else)
    QString codeExe = "C:/Users/rockl/AppData/Local/Programs/Microsoft VS Code/Code.exe";
    QProcess::startDetached(codeExe, args);
}


} 
