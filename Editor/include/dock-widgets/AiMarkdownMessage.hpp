#pragma once

#include <QTextBrowser>

class QResizeEvent;
class QWheelEvent;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QVariantAnimation;

// An assistant-message surface that renders GitHub-style Markdown without
// introducing another scrolling region inside the chat transcript. RockEngine
// reference links remain ordinary anchors; AiChatGui owns their behavior.
class AiMarkdownMessage final : public QTextBrowser {
public:
    explicit AiMarkdownMessage(QWidget* parent = nullptr);

    void SetMarkdownText(const QString& markdown);
    void StartFadeIn();
    void AnimateStreamingUpdate();
    void FinishStreamingFade();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void ApplyDefaultLineSpacing();
    void ApplyHighlights();
    void DecorateReferences();
    void UpdateDocumentWidth();
    void UpdateHeight();

    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_fadeAnimation = nullptr;
    QVariantAnimation* m_streamTextFade = nullptr;
    QString m_previousPlainText;
    QString m_currentPlainText;
};
