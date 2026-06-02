#include "utils/EditorUtils.hpp"
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <vector>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

namespace EditorUtils {

QIcon CustomIconProvider::icon(const QFileInfo &info) const {
    const QString& name = info.fileName();
    
    if (info.isDir()) {
        return QIcon("Domain/lib/assets/icons/folder_icon.png");
    } 

    else if (info.suffix() == "scene"){
        return QIcon("Domain/lib/assets/icons/scene_icon.png");
    }
    else if (info.suffix() == "config"){
        return QIcon("Domain/lib/assets/icons/config_icon.png");
    }
    else if (name.startsWith("Cmake",  Qt::CaseInsensitive)){
        return QIcon("Domain/lib/assets/icons/cmake_icon.png");
    }   
    else if (info.suffix() == "yaml") {
        return QIcon("Domain/lib/assets/icons/yaml_icon.png");
    } else if (info.suffix() == "cpp") {
        return QIcon("Domain/lib/assets/icons/cpp_icon.png");
    } else if (info.suffix() == "hpp") {
        return QIcon("Domain/lib/assets/icons/hpp_icon.png");
    } else if (info.suffix() == "h") {
        return QIcon("Domain/lib/assets/icons/hpp_icon.png");
    } else if (info.suffix() == "vert" || info.suffix() == "frag") {
        return QIcon("Domain/lib/assets/icons/shader_icon.png");
    
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
    QString codeCmd = "C:/Users/rockl/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd";

    QString gotoArg = lineNumber > 0
        ? QString("%1:%2").arg(qPath).arg(lineNumber)
        : qPath;

    // Use setNativeArguments to bypass Qt's quoting so cmd.exe receives
    // the correct /S /C ""path with spaces\code.cmd" -g "file:line"" form.
    QString nativeArgs = QString("/S /C \"\"%1\" -g \"%2\"\"")
        .arg(codeCmd)
        .arg(gotoArg);

    QProcess proc;
    proc.setProgram("cmd.exe");
    proc.setNativeArguments(nativeArgs);
    proc.startDetached();
}


} 
