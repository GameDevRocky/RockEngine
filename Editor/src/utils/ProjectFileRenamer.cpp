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
#include <algorithm>
#include <vector>

namespace {
namespace fs = std::filesystem;

struct PathMove { QString before, after; bool directory = false; };
struct Edit { QString before, after; QByteArray original, replacement; };
struct ModuleMove { QString before, after; };

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

QString remap(const QString& path, const PathMove& move) {
    if (samePath(path, move.before)) return move.after;
    if (move.directory && inside(path, move.before))
        return move.after + path.mid(move.before.size());
    return path;
}

QString remap(const QString& path, const std::vector<PathMove>& moves) {
    QString result = path;
    for (const PathMove& move : moves) result = remap(result, move);
    return result;
}

bool atomicWrite(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(bytes) != bytes.size()) return false;
    return file.commit();
}

bool diskMove(const PathMove& move) {
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

QString renamedModule(const QString& module, const std::vector<ModuleMove>& modules) {
    for (const auto& move : modules)
        if (module == move.before) return move.after;
    return module;
}

bool stageYaml(const QString& path, const std::vector<PathMove>& moves,
               const std::vector<ModuleMove>& modules,
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
        // The destination does not exist while edits are being staged. Use a
        // lexical relative path; filesystem canonicalization may fail here.
        const QString replacement = QDir(QString::fromStdString(
            EngineUtils::GetAssetRoot())).relativeFilePath(moved);
        node[key] = replacement.toStdString();
        changed = true;
    };
    if (isBuild) updateStoredPath("startupScene");
    if (metaExtension(suffix)) {
        updateStoredPath("path");
        updateStoredPath("source_path");
    }
    if (suffix == "scene" && !modules.empty()) {
        YAML::Node components = node["components"];
        if (components && components.IsSequence()) {
            for (YAML::Node component : components) {
                if (component["type"] && component["module"] &&
                    component["type"].as<std::string>() == "ScriptComponent") {
                    const QString current = QString::fromStdString(
                        component["module"].as<std::string>());
                    const QString renamed = renamedModule(current, modules);
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

void updateLoaded(const std::vector<PathMove>& moves,
                  const std::vector<ModuleMove>& modules) {
    for (const PathMove& move : moves)
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
            if (!modules.empty()) {
                for (GameObject* object : scene->GetAllGameObjects()) {
                    if (!object) continue;
                    for (ScriptComponent* script : object->GetComponents<ScriptComponent>())
                        if (script) {
                            const QString current = QString::fromStdString(script->GetScriptModuleName());
                            const QString moved = renamedModule(current, modules);
                            if (moved != current)
                                script->RelocateModule(current.toStdString(), moved.toStdString());
                        }
                }
            }
        }
    }
}

// Shared transaction for rename and drag/drop moves. Stage reference edits
// before touching disk; restore both file locations and bytes on failure.
bool applyMoves(const std::vector<PathMove>& moves, QString* error);

bool planMoves(const QStringList& paths, const QString& directory,
               std::vector<PathMove>& moves, QString* error) {
    auto fail = [&](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const QString target = absolute(directory);
    const QFileInfo destination(target);
    const QString root = sandboxRoot();
    if (!destination.isDir() || destination.isSymLink() || !inside(target, root) ||
        !inside(absolute(destination.canonicalFilePath()),
                absolute(QFileInfo(root).canonicalFilePath())))
        return fail("Choose a folder inside project content.");
    if (paths.isEmpty()) return fail("No files to move.");

    QStringList selected;
    for (const QString& path : paths) {
        const QString source = absolute(path);
        if (!ProjectFileRenamer::CanRename(source))
            return fail("This item cannot be moved: " + source);
        if (std::none_of(selected.begin(), selected.end(), [&](const QString& other) {
                return samePath(source, other);
            })) selected.push_back(source);
    }
    for (const QString& source : selected) {
        // A folder selection already carries all selected descendants.
        if (std::any_of(selected.begin(), selected.end(), [&](const QString& other) {
                return !samePath(source, other) && QFileInfo(other).isDir() && inside(source, other);
            })) continue;
        const QFileInfo info(source);
        if (info.isDir() && inside(target, source))
            return fail("A folder cannot be moved into itself or one of its subfolders.");
        if (samePath(info.absolutePath(), target)) continue;

        auto add = [&](const QString& before) {
            if (std::none_of(moves.begin(), moves.end(), [&](const PathMove& move) {
                    return samePath(move.before, before);
                })) moves.push_back({before, QDir(target).filePath(QFileInfo(before).fileName()),
                                      QFileInfo(before).isDir()});
        };
        if (info.isDir()) { add(source); continue; }
        const QString suffix = info.suffix().toLower();
        const QString metaExt = metaExtension(suffix) ? "." + suffix :
            QString::fromStdString(AssetMetaService::MetaExtensionFor(("." + suffix).toStdString()));
        const QString sourcePath = metaExtension(suffix)
            ? absolute(info.absolutePath() + "/" + info.completeBaseName()) : source;
        const QString descriptor = metaExtension(suffix) ? source : source + metaExt;
        if (metaExtension(suffix) && !QFileInfo::exists(sourcePath))
            return fail("The asset descriptor has no matching source file.");
        if (!metaExt.isEmpty() && QFileInfo::exists(descriptor)) {
            if (!ProjectFileRenamer::CanRename(sourcePath) ||
                !ProjectFileRenamer::CanRename(descriptor))
                return fail("Paired assets cannot contain symbolic links.");
            add(sourcePath);
            add(descriptor);
        } else add(source);
    }
    if (moves.empty()) return fail("The files are already in this folder.");
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const PathMove& move = moves[i];
        if (protectedPath(move.after) || QFileInfo::exists(move.after))
            return fail("The destination already exists: " + move.after);
        for (std::size_t j = 0; j < i; ++j)
            if (samePath(move.after, moves[j].after))
                return fail("Several files have the same destination: " + move.after);
    }
    return true;
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
    std::vector<PathMove> moves;
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
        }
    }

    for (const PathMove& move : moves) {
        if (!inside(move.after, root) || protectedPath(move.after) ||
            (QFileInfo::exists(move.after) && !samePath(move.after, move.before))) {
            if (error) *error = "The destination exists or is outside project content.";
            return false;
        }
    }

    return applyMoves(moves, error);
}

bool ProjectFileRenamer::CanMove(const QStringList& paths, const QString& directory, QString* error) {
    std::vector<PathMove> moves;
    return planMoves(paths, directory, moves, error);
}

bool ProjectFileRenamer::Move(const QStringList& paths, const QString& directory, QString* error) {
    std::vector<PathMove> moves;
    return planMoves(paths, directory, moves, error) && applyMoves(moves, error);
}

namespace {
QString qualifiedModule(const QString& path, const QString& root) {
    QString relative = QDir(root).relativeFilePath(path);
    relative.chop(3); // .py
    return relative.replace('/', '.');
}

bool applyMoves(const std::vector<PathMove>& moves, QString* error) {
    std::vector<Edit> edits;
    std::vector<ModuleMove> modules;
    QStringList yamlFiles;
    const QString root = sandboxRoot();
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
        const QString moved = remap(candidate, moves);
        if (ext == "py" && moved != candidate && inside(candidate, root)) {
            QString relative = QDir(root).relativeFilePath(moved);
            relative.chop(3);
            for (const QString& part : relative.split('/')) {
                if (!validScriptModule(part)) {
                    if (error) *error = "Script destinations must use valid Python module names.";
                    return false;
                }
            }
            const QString newModule = relative.replace('/', '.');
            const QString fullNewModule = "Domain.sandbox." + newModule;
            modules.push_back({"Domain.sandbox." + qualifiedModule(candidate, root), fullNewModule});
            const QString scripts = root + "/scripts";
            if (inside(candidate, scripts)) {
                const QString replacement = inside(moved, scripts)
                    ? qualifiedModule(moved, scripts) : fullNewModule;
                modules.push_back({qualifiedModule(candidate, scripts), replacement});
            }
        }
        if (metaExtension(ext) || ext == "scene" ||
            QFileInfo(candidate).fileName() == "project.build")
            yamlFiles.push_back(candidate);
    }
    if (scanError) {
        if (error) *error = "Cannot scan project content: " +
            QString::fromStdString(scanError.message());
        return false;
    }
    for (const QString& candidate : yamlFiles)
        if (!stageYaml(candidate, moves, modules, edits, error)) return false;

    std::vector<PathMove> completed;
    std::size_t written = 0;
    for (const PathMove& move : moves) {
        if (!diskMove(move)) {
            if (error) *error = "Could not move " + move.before;
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
    updateLoaded(moves, modules);
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
} // namespace
