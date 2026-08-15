#pragma once

#include "ai/AiAgentService.hpp"

#include <QPair>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
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

private:
    enum class LoginAttempt { None, Account, ApiKey };

    struct AuthState {
        bool checked = false;
        bool cliAvailable = false;
        bool authenticated = false;
        QString detail;
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
    void SubmitPrompt();
    void AppendMessage(const QString& role, const QString& text, bool error = false);
    void RenderTranscript();
    void ClearLoginError();
    void ShowLoginError(const QString& detail);

    bool m_initialized = false;
    bool m_busy = false;
    LoginAttempt m_loginAttempt = LoginAttempt::None;
    AuthState m_auth[3];
    QVector<QPair<QString, QString>> m_messages;
    QVector<bool> m_messageErrors;

    QComboBox* m_provider = nullptr;
    QLabel* m_providerStatus = nullptr;
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
    QLabel* m_activity = nullptr;
    QWidget* m_composerFrame = nullptr;
    QPlainTextEdit* m_prompt = nullptr;
    QPushButton* m_send = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_newChat = nullptr;
    QPushButton* m_signOut = nullptr;
};
