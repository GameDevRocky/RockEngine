#include "EditorUtils.hpp"
#include <QProcess>
#include <QString>
#include <QStringList>
#include <vector>

namespace EditorUtils {
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
