#include "utils/EditorUtils.hpp"
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QDebug>
#include <vector>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

namespace EditorUtils {

// Resolve a bundled editor asset (path relative to the project/app root) and load it
// as an icon. Goes through GetAssetPath so it works regardless of the launch directory.
static QIcon AssetIcon(const char* relativePath) {
    return QIcon(QString::fromStdString(EngineUtils::GetAssetPath(relativePath)));
}

QIcon CustomIconProvider::icon(const QFileInfo &info) const {
    const QString& name = info.fileName();
    
    if (info.isDir()) {
        return AssetIcon("Domain/lib/assets/icons/folder_icon.png");
    } 

    else if (info.suffix() == "scene"){
        return AssetIcon("Domain/lib/assets/icons/scene_icon.png");
    }
    else if (info.suffix() == "config"){
        return AssetIcon("Domain/lib/assets/icons/config_icon.png");
    }
    else if (name.startsWith("Cmake",  Qt::CaseInsensitive)){
        return AssetIcon("Domain/lib/assets/icons/cmake_icon.png");
    }   
    else if (info.suffix() == "yaml") {
        return AssetIcon("Domain/lib/assets/icons/yaml_icon.png");
    } else if (info.suffix() == "cpp") {
        return AssetIcon("Domain/lib/assets/icons/cpp_icon.png");
    } else if (info.suffix() == "hpp") {
        return AssetIcon("Domain/lib/assets/icons/hpp_icon.png");
    } else if (info.suffix() == "h") {
        return AssetIcon("Domain/lib/assets/icons/hpp_icon.png");
    } else if (info.suffix() == "shader" || info.suffix() == "glsl") {
        return AssetIcon("Domain/lib/assets/icons/shader_icon.png");

    } else if (info.suffix() == "font" || info.suffix() == "ttf" || info.suffix() == "otf") {
        // The .ttf/.otf cases are for the asset-ref picker and anywhere else a
        // source font is shown directly; the folder view hides those behind their
        // .font meta (see AssetFilterProxyModel).
        return AssetIcon("Domain/lib/assets/icons/font_icon.png");

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
    QString gotoArg = lineNumber > 0
        ? QString("%1:%2").arg(qPath).arg(lineNumber)
        : qPath;

    // Launch the VS Code CLI ("code") from PATH with "-g <file>:<line>".
    // Requires the "Shell Command: Install 'code' command in PATH" to have been run.
#ifdef Q_OS_WIN
    // On Windows "code" is a .cmd shim, which CreateProcess can't run directly,
    // so go through the shell. cmd.exe is the platform shell (no hardcoded paths).
    const bool ok = QProcess::startDetached(
        "cmd", QStringList() << "/c" << "code" << "-g" << gotoArg);
#else
    const bool ok = QProcess::startDetached(
        "code", QStringList() << "-g" << gotoArg);
#endif
    if (!ok) {
        qWarning() << "OpenInVSCode: could not launch the 'code' CLI for" << gotoArg
                   << "- ensure VS Code's 'code' command is on PATH.";
    }
}


} 
