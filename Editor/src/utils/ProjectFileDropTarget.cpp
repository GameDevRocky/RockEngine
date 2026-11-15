#include "utils/ProjectFileDropTarget.hpp"

#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QRubberBand>
#include <QUrl>
#include <QWidget>

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/ProjectFileRenamer.hpp"

namespace {
bool inProject(const QString& path) {
    const QString root = QDir::cleanPath(QString::fromStdString(
        EngineUtils::GetAssetPath("Domain/sandbox")));
    const QString resolved = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    return resolved.compare(root, sensitivity) == 0 ||
           resolved.startsWith(root + '/', sensitivity);
}

bool importable(const QString& path) {
    return !AssetMetaService::MetaExtensionFor(
        ("." + QFileInfo(path).suffix().toLower()).toStdString()).empty();
}

Qt::DropAction actionFor(const QMimeData* mime, QStringList& paths) {
    // Sprite sub-assets have no file to move. Mixed file/sprite drags stay
    // copy-only reference drags rather than moving just part of a selection.
    if (!mime->hasUrls() || mime->hasFormat(kSpriteMimeType) ||
        mime->hasFormat(kSpriteListMimeType)) return Qt::IgnoreAction;
    bool project = false, external = false;
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) return Qt::IgnoreAction;
        const QString path = url.toLocalFile();
        if (inProject(path)) {
            if (!ProjectFileRenamer::CanRename(path)) return Qt::IgnoreAction;
            project = true;
        } else {
            if (!QFileInfo(path).isFile() ||
                (!importable(path) && QFileInfo(path).suffix().toLower() != "material"))
                return Qt::IgnoreAction;
            external = true;
        }
        paths.push_back(path);
    }
    if (paths.empty() || (project && external)) return Qt::IgnoreAction;
    return project ? Qt::MoveAction : Qt::CopyAction;
}
}

ProjectFileDropTarget::ProjectFileDropTarget(QWidget* surface, Resolver resolver)
    : QObject(surface), m_surface(surface),
      m_highlight(new QRubberBand(QRubberBand::Rectangle, surface)),
      m_resolve(std::move(resolver)) {
    m_highlight->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_highlight->hide();
    surface->setAcceptDrops(true);
    surface->installEventFilter(this);
}

bool ProjectFileDropTarget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_surface) return false;
    if (event->type() == QEvent::DragLeave) {
        m_highlight->hide();
        event->accept();
        return true;
    }
    if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove &&
        event->type() != QEvent::Drop) return false;

    auto* drop = static_cast<QDropEvent*>(event);
    m_highlight->hide();
    QStringList paths;
    const Qt::DropAction action = actionFor(drop->mimeData(), paths);
    if (action == Qt::IgnoreAction || !drop->possibleActions().testFlag(action)) {
        drop->ignore();
        return true;
    }
    const Target target = m_resolve(drop->position().toPoint());
    // Enter accepts a supported payload even over a non-target cell, allowing
    // the subsequent drag move to find a folder or ancestor breadcrumb.
    if (event->type() != QEvent::DragEnter &&
        (target.directory.isEmpty() || !QFileInfo(target.directory).isDir() ||
         QFileInfo(target.directory).isSymLink() ||
         !inProject(target.directory) ||
         !inProject(QFileInfo(target.directory).canonicalFilePath()) ||
         (action == Qt::MoveAction && !ProjectFileRenamer::CanMove(paths, target.directory)))) {
        drop->ignore();
        return true;
    }
    if (event->type() == QEvent::Drop) {
        QString error;
        bool handled = false;
        if (action == Qt::MoveAction) {
            handled = ProjectFileRenamer::Move(paths, target.directory, &error);
        } else {
            for (const QString& path : paths) {
                if (importable(path)) {
                    handled |= !AssetManager::Get().ImportAsset(
                        path.toStdString(), target.directory.toStdString()).empty();
                } else {
                    AssetManager::Get().LoadAssetFromFile(path.toStdString());
                    handled = true;
                }
            }
        }
        if (!handled) {
            drop->ignore();
            if (!error.isEmpty()) {
                auto* message = new QMessageBox(QMessageBox::Warning, "Move Failed", error,
                                                QMessageBox::Ok, m_surface);
                message->setAttribute(Qt::WA_DeleteOnClose);
                message->open();
            }
            return true;
        }
    } else if (event->type() == QEvent::DragMove && !target.highlight.isEmpty()) {
        m_highlight->setGeometry(target.highlight.adjusted(1, 1, -1, -1));
        m_highlight->show();
        m_highlight->raise();
    }
    drop->setDropAction(action);
    drop->accept();
    return true;
}
