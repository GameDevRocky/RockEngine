#pragma once

#include "ai/AiAgentService.hpp"

#include <QPair>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QComboBox;
class QAction;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QMimeData;
class QPlainTextEdit;
class QPropertyAnimation;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

class AiChatGui : public QWidget {
    Q_OBJECT
public:
    static AiChatGui* Get();

    void Init();
    void Shutdown();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class LoginAttempt { None, Account, ApiKey };
    enum class AttachmentKind { File, GameObject, Component, Material, Texture, Sprite };

    struct AuthState {
        bool checked = false;
        bool cliAvailable = false;
        bool authenticated = false;
        QString detail;
    };

    struct Attachment {
        AttachmentKind kind = AttachmentKind::File;
        QString key;
        QString title;
        QString subtitle;
        QString id;
        QString path;
    };

    explicit AiChatGui(QWidget* parent = nullptr);
    ~AiChatGui() override = default;

    ai::AiAgentService::Provider CurrentProvider() const;
    int ProviderIndex(ai::AiAgentService::Provider provider) const;
    void BuildUi();
    void ConfigureGradientAnimations();
    void StartGradientAnimation(QPropertyAnimation* animation, QWidget* blob,
                                int sizePercent, int baseDurationMs);
    void RefreshProviderUi();
    void RefreshModelUi();
    void RefreshSettingsMenu();
    QString SelectedModel() const;
    void OpenFilePicker();
    bool CanAttachMimeData(const QMimeData* mime) const;
    bool AttachMimeData(const QMimeData* mime);
    void AttachFile(const QString& path);
    void AttachGameObject(const QString& id);
    void AttachComponent(const QString& id);
    void AttachSprite(const QString& id);
    void AddAttachment(Attachment attachment);
    void RemoveAttachment(const QString& key);
    void ClearAttachments();
    void RenderAttachments();
    void UpdatePromptHeight();
    QString BuildRequestMessage(const QString& prompt) const;
    QString BuildAttachmentContext(const Attachment& attachment) const;
    void SetAttachmentDropActive(bool active);
    void SubmitPrompt();
    void AppendMessage(const QString& role, const QString& text, bool error = false);
    void UpdateStreamingMessage(const QString& text);
    void FinishStreamingMessage(const QString& text);
    void RenderTranscript();
    void ClearLoginError();
    void ShowLoginError(const QString& detail);

    bool m_initialized = false;
    bool m_busy = false;
    LoginAttempt m_loginAttempt = LoginAttempt::None;
    AuthState m_auth[3];
    QVector<QPair<QString, QString>> m_messages;
    QVector<bool> m_messageErrors;
    qsizetype m_streamingMessageIndex = -1;
    QPointer<QLabel> m_streamingBubble;

    QComboBox* m_provider = nullptr;
    QStackedWidget* m_pages = nullptr;

    QWidget* m_loginPage = nullptr;
    QWidget* m_gradientLayer = nullptr;
    QVector<QWidget*> m_gradientBlobs;
    QVector<QPropertyAnimation*> m_gradientAnimations;
    QLabel* m_loginLogo = nullptr;
    QLabel* m_loginHeading = nullptr;
    QLabel* m_loginExplanation = nullptr;
    QLabel* m_loginStatus = nullptr;
    QLabel* m_installLink = nullptr;
    QPushButton* m_accountLogin = nullptr;
    QLabel* m_apiSeparator = nullptr;
    QWidget* m_credentialFrame = nullptr;
    QLineEdit* m_apiKey = nullptr;
    QPushButton* m_apiKeyLogin = nullptr;
    QPushButton* m_loginCancel = nullptr;
    QLabel* m_apiKeyLink = nullptr;

    QScrollArea* m_transcript = nullptr;
    QWidget* m_messageList = nullptr;
    QVBoxLayout* m_messageLayout = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_providerMenu = nullptr;
    QLabel* m_settingsProviderLogo = nullptr;
    QLabel* m_settingsProviderName = nullptr;
    QLabel* m_settingsConnection = nullptr;
    QLabel* m_activity = nullptr;
    QWidget* m_composerFrame = nullptr;
    QScrollArea* m_attachmentScroll = nullptr;
    QWidget* m_attachmentList = nullptr;
    QHBoxLayout* m_attachmentLayout = nullptr;
    QPlainTextEdit* m_prompt = nullptr;
    QComboBox* m_model = nullptr;
    QPushButton* m_addContext = nullptr;
    QPushButton* m_send = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_settings = nullptr;
    QPushButton* m_newChat = nullptr;
    QAction* m_signOut = nullptr;
    QVector<Attachment> m_attachments;
};
