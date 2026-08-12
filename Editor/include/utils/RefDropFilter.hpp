#pragma once
#include <QObject>
#include <QEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QString>
#include <functional>
#include <string>
#include <yaml-cpp/yaml.h>

#include "engine/utils/Properties.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SceneManager.hpp"
#include "engine/core/Scene.hpp"
#include "engine/components/ScriptComponent.hpp"
#include "Engine.hpp"
#include "utils/DragDropMime.hpp"

// Turns an inspector reference widget's container into a drop target. Install it
// on a widget that has setAcceptDrops(true); on a compatible drop it calls
// onDropped(id) with the resolved asset/GameObject id, so the widget assigns it
// exactly as if picked through the "…" picker.
//
// Compatibility is decided from the widget's PropDesc, so only matching types
// drop:
//   * MATERIAL / SPRITE / TEXTURE / SHADER tags accept an asset FILE drag from
//     the Folder view (text/uri-list) whose asset is that exact type.
//   * OBJECT_REF tag accepts a GameObject drag from the Hierarchy
//     (kGameObjectMimeType), further filtered by any script-class
//     (gameobject:<Class>) or native-component (component:<Type>) constraint.
class RefDropFilter : public QObject {
public:
    // `gate` (optional): an extra accept condition evaluated on every drag/drop —
    // when it returns false the drop is rejected (used to restrict the inspector's
    // script-drop target to GameObject selections). No gate = always eligible.
    RefDropFilter(Properties::PropDesc desc,
                  std::function<void(const std::string&)> onDropped,
                  QObject* parent = nullptr,
                  std::function<bool()> gate = nullptr)
        : QObject(parent), m_desc(std::move(desc)),
          m_onDropped(std::move(onDropped)), m_gate(std::move(gate)) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        switch (event->type()) {
            case QEvent::DragEnter:
            case QEvent::DragMove: {
                auto* de = static_cast<QDropEvent*>(event);
                std::string id;
                if (Resolve(de->mimeData(), id)) {
                    // Copy, never Move: a Move result makes the Hierarchy (an
                    // InternalMove view) delete the dragged row it thinks moved.
                    de->setDropAction(Qt::CopyAction);
                    de->accept();
                } else {
                    de->ignore();
                }
                return true;
            }
            case QEvent::Drop: {
                auto* de = static_cast<QDropEvent*>(event);
                std::string id;
                if (Resolve(de->mimeData(), id)) {
                    de->setDropAction(Qt::CopyAction);
                    de->accept();
                    if (m_onDropped) m_onDropped(id);
                } else {
                    de->ignore();
                }
                return true;
            }
            default:
                return QObject::eventFilter(obj, event);
        }
    }

public:
    // Every sprite id carried by a drag, in order — the list type if present,
    // otherwise the single type, otherwise any sprite assets among dropped files.
    // Shared so a list-accepting target (the Animator frame strip) resolves a drop
    // exactly the way this filter would, instead of re-parsing MIME by hand.
    static std::vector<std::string> SpriteIdsFromMime(const QMimeData* mime) {
        std::vector<std::string> ids;
        if (!mime) return ids;

        if (mime->hasFormat(kSpriteListMimeType)) {
            const QString blob = QString::fromUtf8(mime->data(kSpriteListMimeType));
            for (const QString& part : blob.split('\n', Qt::SkipEmptyParts))
                ids.push_back(part.trimmed().toStdString());
            return ids;
        }
        if (mime->hasFormat(kSpriteMimeType)) {
            const std::string id = QString::fromUtf8(mime->data(kSpriteMimeType)).toStdString();
            if (!id.empty()) ids.push_back(id);
            return ids;
        }
        if (mime->hasUrls()) {
            for (const QUrl& url : mime->urls()) {
                const QString path = url.toLocalFile();
                if (path.isEmpty()) continue;
                const std::string id = AssetIdForFile(path);
                if (!id.empty() && AssetManager::Get().GetSprite(id)) ids.push_back(id);
            }
        }
        return ids;
    }

private:
    // Returns true and fills outId when `mime` holds something droppable on a
    // field described by m_desc. Pure query: no side effects.
    bool Resolve(const QMimeData* mime, std::string& outId) const {
        using Properties::Tags;

        if (m_gate && !m_gate()) return false;   // extra caller-supplied gate

        // ── GameObject drag from the Hierarchy ────────────────────────────────
        if (mime->hasFormat(kGameObjectMimeType)) {
            if (m_desc.tag != Tags::OBJECT_REF) return false;   // asset fields reject objects
            std::string goId = QString::fromUtf8(mime->data(kGameObjectMimeType)).toStdString();
            if (goId.empty()) return false;

            GameObject* go = FindGameObject(goId);
            if (!go) return false;

            // gameobject:<Class> — object must carry that user script class.
            if (!m_desc.refClassFilter.empty()) {
                auto* sc = go->GetComponent<ScriptComponent>();
                if (!sc || sc->GetScriptClassName() != m_desc.refClassFilter) return false;
            }
            // component:<Type> — object must have that native component.
            if (!m_desc.componentTypeFilter.empty() &&
                !go->HasComponentByName(m_desc.componentTypeFilter)) return false;

            outId = goId;
            return true;
        }

        // ── Multi-sprite drag (asset picker) ──────────────────────────────────
        // A single-value field takes the first id; targets that can hold a list
        // read kSpriteListMimeType themselves. Checked before the single type so
        // the ordering here matches what a list-aware target would pick.
        if (mime->hasFormat(kSpriteListMimeType)) {
            if (m_desc.tag != Tags::SPRITE) return false;
            for (const std::string& id : SpriteIdsFromMime(mime)) {
                if (AssetManager::Get().GetSprite(id)) { outId = id; return true; }
            }
            return false;
        }

        // ── Sprite drag from the Folder view's texture hover column ───────────
        if (mime->hasFormat(kSpriteMimeType)) {
            if (m_desc.tag != Tags::SPRITE) return false;   // only sprite fields
            std::string spriteId = QString::fromUtf8(mime->data(kSpriteMimeType)).toStdString();
            if (spriteId.empty() || !AssetManager::Get().GetSprite(spriteId)) return false;
            outId = spriteId;
            return true;
        }

        // ── Script .py file drag from the Folder view ─────────────────────────
        if (mime->hasUrls() && m_desc.tag == Tags::SCRIPT) {
            for (const QUrl& url : mime->urls()) {
                const QString path = url.toLocalFile();
                if (QFileInfo(path).suffix().toLower() != "py") continue;
                const std::string ref = ScriptRefForFile(path);
                if (!ref.empty()) { outId = ref; return true; }   // first .py script wins
            }
            return false;
        }

        // ── Asset file drag from the Folder view ──────────────────────────────
        if (mime->hasUrls()) {
            if (m_desc.tag != Tags::MATERIAL && m_desc.tag != Tags::SPRITE &&
                m_desc.tag != Tags::TEXTURE  && m_desc.tag != Tags::SHADER &&
                m_desc.tag != Tags::FONT)
                return false;   // object-ref fields reject asset files

            for (const QUrl& url : mime->urls()) {
                const QString path = url.toLocalFile();
                if (path.isEmpty()) continue;
                const std::string id = AssetIdForFile(path);
                if (!id.empty() && AssetMatchesTag(id, m_desc.tag)) {
                    outId = id;
                    return true;   // first compatible file wins
                }
            }
            return false;
        }

        return false;
    }

    // Resolves a dropped .py file to a "module:class" script reference by matching
    // the file stem against the discovered ScriptableComponent subclasses. Prefers
    // the class named after the file (the common one-class-per-file convention),
    // else the first script class defined in that module. Empty if none.
    static std::string ScriptRefForFile(const QString& path) {
        const std::string stem = QFileInfo(path).completeBaseName().toStdString();
        std::string firstMatch;
        for (const auto& s : ScriptComponent::GetAvailableScripts()) {
            if (s.moduleName != stem) continue;
            if (s.className == stem) return s.moduleName + ":" + s.className;
            if (firstMatch.empty()) firstMatch = s.moduleName + ":" + s.className;
        }
        return firstMatch;
    }

    // Reads the asset id ("id:" field) out of a dropped asset file. Definition/
    // meta files (.mat/.material/.sprite/.png.texture/.vert.shader) carry it
    // directly; a raw image falls back to its sibling ".texture" meta.
    static std::string AssetIdForFile(const QString& path) {
        QString readPath = path;
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp") {
            const QString meta = path + ".texture";
            if (QFileInfo::exists(meta)) readPath = meta;
        }
        try {
            YAML::Node node = YAML::LoadFile(readPath.toStdString());
            if (node["id"]) return node["id"].as<std::string>();
        } catch (...) {}
        return "";
    }

    // True if the asset id resolves to the type the field expects — this is what
    // actually enforces "only drop compatible types" for assets (a material id
    // won't resolve as a sprite, etc.).
    static bool AssetMatchesTag(const std::string& id, Properties::Tags tag) {
        auto& am = AssetManager::Get();
        switch (tag) {
            case Properties::Tags::MATERIAL: return am.GetMaterial(id) != nullptr;
            case Properties::Tags::SPRITE:   return am.GetSprite(id)   != nullptr;
            case Properties::Tags::TEXTURE:  return am.GetTexture(id)  != nullptr;
            case Properties::Tags::SHADER:   return am.GetShader(id)   != nullptr;
            case Properties::Tags::FONT:     return am.GetFont(id)     != nullptr;
            default:                         return false;
        }
    }

    static GameObject* FindGameObject(const std::string& id) {
        auto* container = Engine::Get()->GetActiveContainer();
        if (!container) return nullptr;
        auto* sm = container->FindSystem<SceneManager>();
        if (!sm) return nullptr;
        for (auto* scene : sm->GetScenes())
            for (auto* go : scene->GetAllGameObjects())
                if (go->GetID() == id) return go;
        return nullptr;
    }

    Properties::PropDesc m_desc;
    std::function<void(const std::string&)> m_onDropped;
    std::function<bool()> m_gate;
};
