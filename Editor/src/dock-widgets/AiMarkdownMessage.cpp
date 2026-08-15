#include "dock-widgets/AiMarkdownMessage.hpp"

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Resource.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/EditorUtils.hpp"

#include <QAbstractTextDocumentLayout>
#include <QDir>
#include <QEasingCurve>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr int kReferenceIconSize = 13;

QString ResolveReferencePath(const QString& path) {
    if (path.isEmpty()) return {};
    if (QDir::isAbsolutePath(path)) return QDir::cleanPath(path);
    return QDir(QString::fromUtf8(PROJECT_ROOT)).absoluteFilePath(path);
}

QIcon FileIcon(const QString& path) {
    static const EditorUtils::CustomIconProvider provider;
    return provider.icon(QFileInfo(ResolveReferencePath(path)));
}

Resource* ResourceForReference(const QString& id) {
    if (id.isEmpty()) return nullptr;
    AssetManager& assets = AssetManager::Get();
    const std::string key = id.toStdString();
    if (auto* material = assets.GetMaterial(key)) return material;
    if (auto* texture = assets.GetTexture(key)) return texture;
    if (auto* sprite = assets.GetSprite(key)) return sprite;
    if (auto* shader = assets.GetShader(key)) return shader;
    return nullptr;
}

QIcon ReferenceIcon(const QUrl& url) {
    const QString kind = url.host().toLower();
    if (kind == QStringLiteral("object"))
        return EditorUtils::CustomIconProvider::gameObjectIcon();

    if (kind == QStringLiteral("component")) {
        const QString type = QUrlQuery(url).queryItemValue(
            QStringLiteral("type"), QUrl::FullyDecoded);
        QIcon icon = EditorUtils::CustomIconProvider::componentIcon(type.toStdString());
        return icon.isNull() ? EditorUtils::CustomIconProvider::gameObjectIcon() : icon;
    }

    if (kind == QStringLiteral("file")) {
        const QUrlQuery query(url);
        QString path = query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded);
        if (path.isEmpty()) path = url.path(QUrl::FullyDecoded).mid(1);
        return FileIcon(path);
    }

    if (kind == QStringLiteral("asset")) {
        const QString id = url.path(QUrl::FullyDecoded).mid(1);
        if (Resource* resource = ResourceForReference(id)) {
            QString path = QString::fromStdString(resource->GetFilePath());
            if (auto* texture = dynamic_cast<Texture2D*>(resource); texture && !texture->GetPath().empty())
                path = QString::fromStdString(texture->GetPath());
            else if (auto* sprite = dynamic_cast<Sprite*>(resource)) {
                if (Texture2D* texture = sprite->GetTexture(); texture && !texture->GetPath().empty())
                    path = QString::fromStdString(texture->GetPath());
            }
            if (!path.isEmpty()) return FileIcon(path);
        }
    }

    if (kind == QStringLiteral("scene"))
        return FileIcon(QStringLiteral("reference.scene"));

    return {};
}

bool IsRockEngineReference(const QString& href) {
    return QUrl(href).scheme().compare(QStringLiteral("rockengine"),
                                      Qt::CaseInsensitive) == 0;
}

} // namespace

AiMarkdownMessage::AiMarkdownMessage(QWidget* parent)
    : QTextBrowser(parent) {
    setObjectName(QStringLiteral("AiAssistantBubble"));
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::ClickFocus);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);
    m_fadeAnimation = new QPropertyAnimation(
        m_opacityEffect, "opacity", this);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutSine);

    document()->setDocumentMargin(2.0);
    QTextOption option = document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document()->setDefaultTextOption(option);

    connect(document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged,
            this, [this](const QSizeF&) { UpdateHeight(); });
}

void AiMarkdownMessage::SetMarkdownText(const QString& markdown) {
    if (m_streamTextFade) {
        m_streamTextFade->stop();
        m_streamTextFade->deleteLater();
        m_streamTextFade = nullptr;
    }
    m_previousPlainText = m_currentPlainText;
    document()->setDefaultFont(font());
    document()->setDefaultStyleSheet(QStringLiteral(R"(
        a { color: #4fbd72; text-decoration: none; }
        p { margin-top: 3px; margin-bottom: 7px; }
        ul, ol { margin-top: 3px; margin-bottom: 7px; }
        li { margin-bottom: 2px; }
        code { color: #e4e4e6; background-color: #373739; font-family: Consolas; }
        pre { color: #e4e4e6; background-color: #29292b; margin: 6px 0px; padding: 7px; }
        blockquote { color: #b8b8bc; border-left: 3px solid #4fbd72; margin-left: 4px; padding-left: 8px; }
        table { border-collapse: collapse; margin-top: 6px; margin-bottom: 8px; }
        th { color: #ededee; background-color: #363638; border: 1px solid #505053; padding: 5px 7px; }
        td { border: 1px solid #48484b; padding: 5px 7px; }
    )"));
    document()->setMarkdown(
        markdown,
        QTextDocument::MarkdownFeatures{
            QTextDocument::MarkdownDialectGitHub,
            QTextDocument::MarkdownNoHTML});
    ApplyDefaultLineSpacing();
    ApplyHighlights();
    DecorateReferences();
    m_currentPlainText = document()->toPlainText();
    UpdateDocumentWidth();
    UpdateHeight();
}

void AiMarkdownMessage::StartFadeIn() {
    m_fadeAnimation->stop();
    m_opacityEffect->setOpacity(0.0);
    m_fadeAnimation->setDuration(550);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void AiMarkdownMessage::AnimateStreamingUpdate() {
    int unchangedPrefix = 0;
    const int comparableLength = qMin(m_previousPlainText.size(),
                                      m_currentPlainText.size());
    while (unchangedPrefix < comparableLength &&
           m_previousPlainText[unchangedPrefix] ==
               m_currentPlainText[unchangedPrefix]) {
        ++unchangedPrefix;
    }
    if (unchangedPrefix >= m_currentPlainText.size()) return;

    struct FadeSpan {
        int start = 0;
        int length = 0;
        QColor color;
        qreal originalAlpha = 1.0;
    };
    QVector<FadeSpan> spans;
    const int fadeEnd = document()->characterCount() - 1;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        for (auto iterator = block.begin(); !iterator.atEnd(); ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid() || fragment.charFormat().isImageFormat()) continue;

            const int start = qMax(unchangedPrefix, fragment.position());
            const int end = qMin(fadeEnd, fragment.position() + fragment.length());
            if (end <= start) continue;

            const QBrush foreground = fragment.charFormat().foreground();
            QColor color = foreground.style() == Qt::NoBrush
                ? palette().color(QPalette::Text)
                : foreground.color();
            if (!color.isValid()) color = palette().color(QPalette::Text);
            spans.push_back({start, end - start, color, color.alphaF()});
        }
    }
    if (spans.isEmpty()) return;

    const auto applyOpacity = [this, spans](qreal opacity) {
        for (const FadeSpan& span : spans) {
            if (span.start + span.length >= document()->characterCount()) continue;
            QColor color = span.color;
            color.setAlphaF(qBound(0.0, span.originalAlpha * opacity, 1.0));
            QTextCharFormat format;
            format.setForeground(color);
            QTextCursor cursor(document());
            cursor.setPosition(span.start);
            cursor.setPosition(span.start + span.length,
                               QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(format);
        }
    };

    applyOpacity(0.0);
    auto* animation = new QVariantAnimation(this);
    m_streamTextFade = animation;
    animation->setDuration(320);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::InOutSine);
    connect(animation, &QVariantAnimation::valueChanged, this,
            [applyOpacity](const QVariant& value) {
                applyOpacity(value.toReal());
            });
    connect(animation, &QVariantAnimation::finished, this,
            [this, animation] {
                if (m_streamTextFade == animation) m_streamTextFade = nullptr;
                animation->deleteLater();
            });
    animation->start();
}

void AiMarkdownMessage::FinishStreamingFade() {
    const qreal opacity = m_opacityEffect->opacity();
    m_fadeAnimation->stop();
    if (opacity >= 0.999) {
        m_opacityEffect->setOpacity(1.0);
        return;
    }
    m_fadeAnimation->setDuration(220);
    m_fadeAnimation->setStartValue(opacity);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void AiMarkdownMessage::ApplyDefaultLineSpacing() {
    constexpr qreal lineHeightPercent = 115.0;
    QTextCursor cursor(document());
    cursor.beginEditBlock();
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(lineHeightPercent,
                             QTextBlockFormat::ProportionalHeight);
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();
}

void AiMarkdownMessage::ApplyHighlights() {
    // Qt's Markdown dialect does not define ==highlight==. Support that familiar
    // extension after parsing so raw HTML can remain disabled for model output.
    const QString plainText = document()->toPlainText();
    static const QRegularExpression highlightPattern(
        QStringLiteral("==([^=\\n]+)=="));

    QVector<QRegularExpressionMatch> matches;
    auto iterator = highlightPattern.globalMatch(plainText);
    while (iterator.hasNext()) matches.push_back(iterator.next());

    QTextCharFormat highlight;
    highlight.setBackground(QColor(71, 78, 55));
    highlight.setForeground(QColor(226, 235, 197));

    for (auto match = matches.crbegin(); match != matches.crend(); ++match) {
        QTextCursor cursor(document());
        cursor.setPosition(match->capturedStart(0));
        cursor.setPosition(match->capturedEnd(0), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(match->captured(1), highlight);
    }
}

void AiMarkdownMessage::DecorateReferences() {
    struct Anchor {
        int position = 0;
        int end = 0;
        QString href;
        QTextCharFormat format;
    };

    QVector<Anchor> anchors;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        for (auto iterator = block.begin(); !iterator.atEnd(); ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid()) continue;
            const QTextCharFormat format = fragment.charFormat();
            if (!format.isAnchor() || !IsRockEngineReference(format.anchorHref())) continue;

            if (!anchors.isEmpty() && anchors.back().href == format.anchorHref() &&
                anchors.back().end == fragment.position()) {
                anchors.back().end = fragment.position() + fragment.length();
                continue;
            }
            anchors.push_back({fragment.position(), fragment.position() + fragment.length(),
                               format.anchorHref(), format});
        }
    }

    for (auto anchor = anchors.crbegin(); anchor != anchors.crend(); ++anchor) {
        const QUrl link(anchor->href);
        const QIcon icon = ReferenceIcon(link);
        if (icon.isNull()) continue;

        const QUrl resourceUrl(QStringLiteral("rockengine-icon://reference/%1")
                                   .arg(anchor->position));
        document()->addResource(QTextDocument::ImageResource, resourceUrl,
                                icon.pixmap(kReferenceIconSize, kReferenceIconSize));

        QTextImageFormat imageFormat;
        imageFormat.setName(resourceUrl.toString());
        imageFormat.setWidth(kReferenceIconSize);
        imageFormat.setHeight(kReferenceIconSize);
        imageFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
        imageFormat.setAnchor(true);
        imageFormat.setAnchorHref(anchor->href);

        QTextCursor cursor(document());
        cursor.setPosition(anchor->position);
        cursor.insertImage(imageFormat);
        cursor.insertText(QStringLiteral("\u2009"), anchor->format);
    }
}

void AiMarkdownMessage::UpdateDocumentWidth() {
    const int width = viewport()->width();
    if (width > 0 && !qFuzzyCompare(document()->textWidth(), qreal(width)))
        document()->setTextWidth(width);
}

void AiMarkdownMessage::UpdateHeight() {
    if (!document()->documentLayout()) return;
    const int contentHeight = static_cast<int>(std::ceil(
        document()->documentLayout()->documentSize().height()));
    const QMargins margins = viewportMargins();
    const int chrome = margins.top() + margins.bottom() + frameWidth() * 2;
    const int target = qMax(fontMetrics().height(), contentHeight + chrome + 2);
    if (height() != target) setFixedHeight(target);
}

void AiMarkdownMessage::resizeEvent(QResizeEvent* event) {
    QTextBrowser::resizeEvent(event);
    UpdateDocumentWidth();
    QTimer::singleShot(0, this, [this]() { UpdateHeight(); });
}

void AiMarkdownMessage::wheelEvent(QWheelEvent* event) {
    // The transcript owns scrolling. Ignoring the wheel event lets it propagate
    // instead of creating a dead wheel zone over a Markdown response.
    event->ignore();
}
