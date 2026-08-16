#include "utils/EditorUtils.hpp"
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QPainter>
#include <QDebug>
#include <vector>
#include <cctype>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

namespace EditorUtils {

// Every bundled editor icon lives in this one directory, so the loaders below take
// a bare file name rather than repeating the path at ~40 call sites.
static constexpr const char* kIconDir = "Domain/lib/assets/icons/";

// Resolve a bundled icon by file name. Goes through GetAssetPath so it works
// regardless of the launch directory, and from a bundled build where the assets sit
// next to the executable instead of under PROJECT_ROOT.
static std::string IconPath(const std::string& fileName) {
    return EngineUtils::GetAssetPath(kIconDir + fileName);
}

static QIcon AssetIcon(const std::string& fileName) {
    return QIcon(QString::fromStdString(IconPath(fileName)));
}

// Same as AssetIcon, but recolors the source image's opaque pixels to a flat
// color while preserving its alpha shape. Several bundled icons are authored
// dark-on-transparent (fine on a light background); this lets them still read
// correctly against the editor's dark panels without editing the source PNG.
static QIcon TintedAssetIcon(const std::string& fileName, const QColor& color) {
    const QPixmap source(QString::fromStdString(IconPath(fileName)));
    if (source.isNull()) return {};
    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(source.rect(), color);
    painter.end();
    return QIcon(tinted);
}

QIcon CustomIconProvider::gameObjectIcon() {
    return AssetIcon("cube.png");
}

QIcon CustomIconProvider::componentIcon(const std::string& typeName) {
    if (typeName == "Transform")
        return AssetIcon("transform_icon.png");
    if (typeName == "SpriteRenderer")
        return AssetIcon("sprite_renderer_icon.png");
    if (typeName == "TextRenderer")
        return AssetIcon("text_renderer_icon.png");
    if (typeName == "ScriptComponent")
        return AssetIcon("python_script_icon.png");
    if (typeName == "RigidBody")
        return AssetIcon("rigidbody_icon.png");
    if (typeName == "BoxCollider")
        return AssetIcon("box_collider_icon.png");
    if (typeName == "CircleCollider")
        return AssetIcon("circle_collider_icon.png");
    if (typeName == "CapsuleCollider")
        return AssetIcon("capsule_collider_icon.png");
    if (typeName == "Animator")
        return AssetIcon("animator_icon.png");
    if (typeName == "Camera")
        return AssetIcon("camera_icon.png");
    if (typeName == "ParticleComponent")
        return AssetIcon("particle_icons.png");
    if (typeName == "Light")
        return AssetIcon("light_icon.png");
    if (typeName == "ShadowCaster")
        return AssetIcon("shadow_caster_icon.png");
    if (typeName == "AudioSource")
        return AssetIcon("audio_source_icon.png");
    if (typeName == "AudioListener")
        return AssetIcon("audio_listener_icon.png");
    if (typeName == "Joint" || typeName.ends_with("Joint"))
        return AssetIcon("joint_icon.png");
    return {};
}

QIcon CustomIconProvider::assetIcon(const std::string& typeName) {
    if (typeName == "AudioClip")
        return AssetIcon("audio_clip_icon.png");
    if (typeName == "Font")
        return AssetIcon("font_icon.png");
    if (typeName == "Material")
        return AssetIcon("material_icon.png");
    if (typeName == "Texture2D")
        return AssetIcon("texture_icon.png");
    if (typeName == "Shader")
        return AssetIcon("shader_icon.png");
    if (typeName == "Scene")
        return AssetIcon("scene_icon.png");
    if (typeName == "PythonScript" || typeName == "Script")
        return AssetIcon("python_script_icon.png");
    return {};
}

QIcon CustomIconProvider::aiSendIcon() {
    return AssetIcon("up_arrow_icon.png");
}

QIcon CustomIconProvider::aiSettingsIcon() {
    // Matches QPushButton#AiTopAction's resting text color in ai_assistant.qss,
    // so this icon-based button reads the same as its glyph-text siblings.
    return TintedAssetIcon("settings_icon.png", QColor(170, 170, 174));
}

QIcon CustomIconProvider::disclosureArrowRightIcon() {
    return AssetIcon("scene_tree_arrow_right.svg");
}

QIcon CustomIconProvider::disclosureArrowDownIcon() {
    return AssetIcon("scene_tree_arrow_down.svg");
}

QIcon CustomIconProvider::icon(const QFileInfo &info) const {
    const QString& name = info.fileName();
    const QString suffix = info.suffix().toLower();
    
    if (info.isDir()) {
        return AssetIcon("folder_icon.png");
    } 

    else if (suffix == "scene"){
        return AssetIcon("scene_icon.png");
    }
    else if (suffix == "config"){
        return AssetIcon("config_icon.png");
    }
    // The .material/.texture metas are what the folder view actually lists — a
    // source .png is hidden behind its .texture meta (see AssetFilterProxyModel),
    // so these are the entries a user sees and they share the Inspector's icons.
    else if (suffix == "material"){
        return AssetIcon("material_icon.png");
    }
    else if (suffix == "texture"){
        return AssetIcon("texture_icon.png");
    }
    else if (name.startsWith("Cmake",  Qt::CaseInsensitive)){
        return AssetIcon("cmake_icon.png");
    }   
    else if (suffix == "yaml") {
        return AssetIcon("yaml_icon.png");
    } else if (suffix == "cpp") {
        return AssetIcon("cpp_icon.png");
    } else if (suffix == "hpp") {
        return AssetIcon("hpp_icon.png");
    } else if (suffix == "h") {
        return AssetIcon("hpp_icon.png");
    } else if (suffix == "shader" || suffix == "glsl") {
        return AssetIcon("shader_icon.png");
    } else if (suffix == "font" || suffix == "ttf" || suffix == "otf") {
        // The .ttf/.otf cases are for the asset-ref picker and anywhere else a
        // source font is shown directly; the folder view hides those behind their
        // .font meta (see AssetFilterProxyModel).
        return AssetIcon("font_icon.png");
    } else if (suffix == "audio" || suffix == "wav" || suffix == "mp3" ||
               suffix == "ogg" || suffix == "flac") {
        return AssetIcon("audio_clip_icon.png");
    } else if (suffix == "py" || suffix == "pyw") {
        return AssetIcon("python_script_icon.png");
    } else if (suffix == "png" || suffix == "jpg" || suffix == "jpeg") {
        // Load the actual image file as a thumbnail
        return QIcon(info.absoluteFilePath());
    }
    // Fallback to default file icon
    return QFileIconProvider::icon(info);
}

std::string FormatLabel(const std::string& raw)
{
    // Trailing separator the call site may already have written ("Color: ") comes off
    // first, so appending one afterwards can't double it up.
    std::size_t end = raw.size();
    while (end > 0) {
        const unsigned char c = static_cast<unsigned char>(raw[end - 1]);
        if (c == ':' || std::isspace(c)) --end;
        else break;
    }

    // Pass 1 -- insert the missing word breaks.
    std::string spaced;
    spaced.reserve(end + 8);
    for (std::size_t i = 0; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == '_' || c == '-') { spaced.push_back(' '); continue; }

        // Only an uppercase letter can open a new word mid-token, and only when the
        // previous character isn't already a break.
        if (std::isupper(c) && !spaced.empty() && spaced.back() != ' ') {
            const unsigned char prev = static_cast<unsigned char>(raw[i - 1]);
            // "startOffset" -> "start Offset"
            const bool afterLowerOrDigit = std::islower(prev) || std::isdigit(prev);
            // "UVMin" -> "UV Min": an acronym ends at the last capital before a
            // lowercase letter, so the break goes there rather than after every capital
            // (which would shred "UV" into "U V").
            const bool acronymEnd = std::isupper(prev) && i + 1 < end &&
                                    std::islower(static_cast<unsigned char>(raw[i + 1]));
            if (afterLowerOrDigit || acronymEnd) spaced.push_back(' ');
        }
        spaced.push_back(static_cast<char>(c));
    }

    // Pass 2 -- collapse whitespace runs and capitalise the head of every alphabetic
    // run. Only the first letter is touched; the rest is left as authored so "UV" and
    // "X" stay intact.
    std::string out;
    out.reserve(spaced.size());
    bool wordStart = true;
    for (const char ch : spaced) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isspace(c)) {
            if (!out.empty() && out.back() != ' ') out.push_back(' ');
            wordStart = true;
        } else if (std::isalpha(c)) {
            out.push_back(wordStart ? static_cast<char>(std::toupper(c)) : ch);
            wordStart = false;
        } else {
            // Punctuation ('(', ',', '/') opens a new word, so "(min,max)" reads
            // "(Min,Max)" rather than "(Min,max)".
            out.push_back(ch);
            wordStart = true;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
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
