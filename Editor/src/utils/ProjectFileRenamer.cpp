#include "utils/ProjectFileRenamer.hpp"

#include "Engine.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <vector>

namespace {
namespace fs = std::filesystem;

struct Move { QString before, after; bool directory = false; };
struct Edit { QString before, after; QByteArray original, replacement; };

QString absolute(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString& a, const QString& b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

bool inside(const QString& path, const QString& root) {
    if (samePath(path, root)) return true;
    return path.size() > root.size() &&
        samePath(path.left(root.size()), root) &&
        (path[root.size()] == '/' || path[root.size()] == '\\');
}

QString remap(const QString& path, const Move& move) {
    if (samePath(path, move.before)) return move.after;
    if (move.directory && inside(path, move.before))
        return move.after + path.mid(move.before.size());
    return path;
}

QString remap(const QString& path, const std::vector<Move>& moves) {
    QString result = path;
    for (const Move& move : moves) result = remap(result, move);
    return result;
}

bool atomicWrite(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(bytes) != bytes.size()) return false;
    return file.commit();
}

bool diskMove(const Move& move) {
    std::error_code ec;
#ifdef Q_OS_WIN
    fs::rename(fs::path(move.before.toStdWString()), fs::path(move.after.toStdWString()), ec);
#else
    fs::rename(fs::path(move.before.toStdString()), fs::path(move.after.toStdString()), ec);
#endif
    return !ec;
}

bool validName(const QString& name) {
    if (name.isEmpty() || name == "." || name == ".." || name != name.trimmed() ||
        name.endsWith('.') || name.contains(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])")))
        return false;
    static const QRegularExpression reserved(
        R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))",
        QRegularExpression::CaseInsensitiveOption);
    return !reserved.match(name).hasMatch();
}

bool validScriptModule(const QString& name) {
    static const QRegularExpression identifier(R"(^[A-Za-z_][A-Za-z_0-9]*$)");
    static const QSet<QString> keywords = {
        "False", "None", "True", "and", "as", "assert", "async", "await", "break",
        "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
        "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
    };
    return identifier.match(name).hasMatch() && !keywords.contains(name);
}

QString sandboxRoot() {
    return absolute(QString::fromStdString(EngineUtils::GetAssetPath("Domain/sandbox")));
}

bool protectedPath(const QString& path) {
    const QString root = sandboxRoot();
    return samePath(path, root) || samePath(path, root + "/scripts") ||
           samePath(path, root + "/project.build");
}

bool metaExtension(const QString& ext) {
    return ext == "texture" || ext == "shader" || ext == "font" || ext == "audio";
}

QString renamedModule(const QString& module, const QString& oldName,
                      const QString& newName) {
    if (module == oldName) return newName;
    const QString prefix = "Domain.sandbox.scripts.";
    if (module == prefix + oldName) return prefix + newName;
    return module;
}

bool stageYaml(const QString& path, const std::vector<Move>& moves,
               const QString& oldModule, const QString& newModule,
               std::vector<Edit>& edits, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = "Cannot read " + path;
        return false;
    }
    const QByteArray original = file.readAll();
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool isBuild = QFileInfo(path).fileName() == "project.build";
    YAML::Node node;
    try {
        node = YAML::Load(original.toStdString());
    } catch (const std::exception& e) {
        if (remap(path, moves) != path) {
            if (error) *error = "Cannot parse " + path + ": " + e.what();
            return false;
        }
        return true;
    }
    if (!node.IsMap()) {
        if (remap(path, moves) != path && metaExtension(suffix)) {
            if (error) *error = "Invalid asset descriptor: " + path;
            return false;
        }
        return true;
    }

    bool changed = false;
    auto updateStoredPath = [&](const char* key) {
        if (!node[key] || !node[key].IsScalar()) return;
        const QString stored = QString::fromStdString(node[key].as<std::string>());
        const QString resolved = absolute(QFileInfo(stored).isAbsolute() ? stored :
            QString::fromStdString(EngineUtils::GetAssetPath(stored.toStdString())));
        const QString moved = remap(resolved, moves);
        if (samePath(moved, resolved) && moved == resolved) return;
        const QString replacement = QString::fromStdString(
            EngineUtils::ToAssetRelative(moved.toStdString()));
        node[key] = replacement.toStdString();
        changed = true;
    };
    if (isBuild) updateStoredPath("startupScene");
    if (metaExtension(suffix)) {
        updateStoredPath("path");
        updateStoredPath("source_path");
    }
    if (suffix == "scene" && !oldModule.isEmpty()) {
        YAML::Node components = node["components"];
        if (components && components.IsSequence()) {
            for (YAML::Node component : components) {
                if (component["type"] && component["module"] &&
                    component["type"].as<std::string>() == "ScriptComponent") {
                    const QString current = QString::fromStdString(
                        component["module"].as<std::string>());
                    const QString renamed = renamedModule(current, oldModule, newModule);
                    if (renamed != current) {
                        component["module"] = renamed.toStdString();
                        changed = true;
                    }
                }
            }
        }
    }
    if (!changed) return true;
    YAML::Emitter emitter;
    emitter << node;
    if (!emitter.good()) {
        if (error) *error = "Cannot serialize " + path;
        return false;
    }
    QByteArray replacement(emitter.c_str());
    if (original.endsWith('\n')) replacement.append('\n');
    edits.push_back({path, remap(path, moves), original, replacement});
    return true;
}

void updateLoaded(const std::vector<Move>& moves,
                  const QString& oldModule, const QString& newModule) {
    for (const Move& move : moves)
        AssetManager::Get().RelocatePaths(move.before.toStdString(), move.after.toStdString(),
                                          move.directory);
    Engine* engine = Engine::Get();
    if (!engine) return;
    for (Container* container : {engine->GetEditorContainer(), engine->GetRuntimeContainer()}) {
        if (!container) continue;
        SceneManager* manager = container->FindSystem<SceneManager>();
        if (!manager) continue;
        for (Scene* scene : manager->GetScenes()) {
            if (!scene) continue;
            QString scenePath = absolute(QString::fromStdString(scene->GetPath()));
            const QString moved = remap(scenePath, moves);
            if (moved != scenePath) scene->SetPath(moved.toStdString());
            if (!oldModule.isEmpty()) {
                for (GameObject* object : scene->GetAllGameObjects()) {
                    if (!object) continue;
                    for (ScriptComponent* script : object->GetComponents<ScriptComponent>())
                        if (script) {
                            script->RelocateModule(oldModule.toStdString(), newModule.toStdString());
                            const std::string prefix = "Domain.sandbox.scripts.";
                            script->RelocateModule(prefix + oldModule.toStdString(),
                                                    prefix + newModule.toStdString());
                        }
                }
            }
        }
    }
}
} // namespace

QString ProjectFileRenamer::EditableName(const QString& path) {
    const QFileInfo info(path);
    if (info.isDir()) return info.fileName();
    if (metaExtension(info.suffix().toLower())) {
        const QFileInfo source(info.absolutePath() + "/" + info.completeBaseName());
        return source.completeBaseName();
    }
    return info.completeBaseName();
}

bool ProjectFileRenamer::CanRename(const QString& path) {
    const QString resolved = absolute(path);
    const QString root = sandboxRoot();
    const QFileInfo info(resolved);
    return info.exists() && !info.isSymLink() && inside(resolved, root) &&
           inside(absolute(info.canonicalFilePath()),
                  absolute(QFileInfo(root).canonicalFilePath())) &&
           !protectedPath(resolved);
}

bool ProjectFileRenamer::Rename(const QString& path, const QString& newName, QString* error) {
    const QString oldPath = absolute(path);
    if (!CanRename(oldPath)) {
        if (error) *error = "This item cannot be renamed.";
        return false;
    }
    if (!validName(newName)) {
        if (error) *error = "Enter a valid filename without an extension.";
        return false;
    }
    if (newName == EditableName(oldPath)) return true;

    const QFileInfo info(oldPath);
    const QString root = sandboxRoot();
    const QString parent = info.absolutePath();
    const QString suffix = info.suffix().toLower();
    std::vector<Move> moves;
    QString oldModule, newModule;
    if (info.isDir()) {
        moves.push_back({oldPath, absolute(parent + "/" + newName), true});
    } else {
        const QString sourcePath = metaExtension(suffix)
            ? absolute(parent + "/" + info.completeBaseName()) : oldPath;
        const QString sourceExt = QFileInfo(sourcePath).suffix();
        const QString metaExt = metaExtension(suffix) ? "." + suffix :
            QString::fromStdString(AssetMetaService::MetaExtensionFor(
                ("." + suffix).toStdString()));
        const QString descriptor = metaExtension(suffix) ? oldPath : oldPath + metaExt;
        const bool paired = !metaExt.isEmpty() && QFileInfo(sourcePath).exists() &&
                            QFileInfo(descriptor).exists();
        if (paired && (QFileInfo(sourcePath).isSymLink() ||
                       QFileInfo(descriptor).isSymLink())) {
            if (error) *error = "Paired assets cannot contain symbolic links.";
            return false;
        }
        if (metaExtension(suffix) && !paired) {
            if (error) *error = "The asset descriptor has no matching source file.";
            return false;
        }
        const QString newSource = absolute(parent + "/" + newName +
            (sourceExt.isEmpty() ? QString() : "." + sourceExt));
        if (paired) {
            moves.push_back({sourcePath, newSource, false});
            moves.push_back({descriptor, newSource + metaExt, false});
        } else {
            moves.push_back({oldPath, absolute(parent + "/" + newName +
                (info.suffix().isEmpty() ? QString() : "." + info.suffix())), false});
        }
        if (suffix == "py" && samePath(parent, root + "/scripts")) {
            if (!validScriptModule(newName)) {
                if (error) *error = "Script filenames must be valid Python module names.";
                return false;
            }
            oldModule = info.completeBaseName();
            newModule = newName;
        }
    }

    for (const Move& move : moves) {
        if (!inside(move.after, root) || protectedPath(move.after) ||
            (QFileInfo::exists(move.after) && !samePath(move.after, move.before))) {
            if (error) *error = "The destination exists or is outside project content.";
            return false;
        }
    }

    std::vector<Edit> edits;
    const QString domainRoot = absolute(QString::fromStdString(
        EngineUtils::GetAssetPath("Domain")));
    std::error_code scanError;
#ifdef Q_OS_WIN
    fs::recursive_directory_iterator it(fs::path(domainRoot.toStdWString()), scanError), end;
#else
    fs::recursive_directory_iterator it(fs::path(domainRoot.toStdString()), scanError), end;
#endif
    for (; !scanError && it != end; it.increment(scanError)) {
        if (it->is_symlink() || !it->is_regular_file()) continue;
#ifdef Q_OS_WIN
        const QString candidate = absolute(QString::fromStdWString(it->path().wstring()));
#else
        const QString candidate = absolute(QString::fromStdString(it->path().string()));
#endif
        const QString ext = QFileInfo(candidate).suffix().toLower();
        if (metaExtension(ext) || ext == "scene" ||
            QFileInfo(candidate).fileName() == "project.build") {
            if (!stageYaml(candidate, moves, oldModule, newModule, edits, error)) return false;
        }
    }
    if (scanError) {
        if (error) *error = "Cannot scan project content: " +
            QString::fromStdString(scanError.message());
        return false;
    }

    std::vector<Move> completed;
    std::size_t written = 0;
    for (const Move& move : moves) {
        if (!diskMove(move)) {
            if (error) *error = "Could not rename " + move.before;
            goto rollback;
        }
        completed.push_back(move);
    }
    for (const Edit& edit : edits) {
        if (!atomicWrite(edit.after, edit.replacement)) {
            if (error) *error = "Could not update " + edit.after;
            goto rollback;
        }
        ++written;
    }
    updateLoaded(moves, oldModule, newModule);
    return true;

rollback:
    while (written > 0) {
        const Edit& edit = edits[--written];
        if (!atomicWrite(edit.after, edit.original) && error)
            *error += " Failed to restore " + edit.after;
    }
    for (auto iter = completed.rbegin(); iter != completed.rend(); ++iter) {
        if (!diskMove({iter->after, iter->before, iter->directory}) && error)
            *error += " Failed to restore " + iter->before;
    }
    return false;
}
