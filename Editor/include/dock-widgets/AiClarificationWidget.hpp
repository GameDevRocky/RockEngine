#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

// Inline answer bank rendered as a normal AI Assistant transcript item. Pending
// questions stay expanded; answered/cancelled questions start collapsed but retain
// their complete read-only contents for later inspection.
class AiClarificationWidget final : public QWidget {
    Q_OBJECT
public:
    explicit AiClarificationWidget(const QJsonObject& request,
                                   const QString& status,
                                   const QJsonObject& answer,
                                   bool expanded,
                                   QWidget* parent = nullptr);

Q_SIGNALS:
    void AnswerSubmitted(const QStringList& selectedIds, const QString& otherText);
    void CancelRequested();
    void ExpansionChanged(bool expanded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void AddOption(QVBoxLayout* layout, const QJsonObject& option,
                   const QStringList& selectedIds);
    void RefreshContinue();
    void RefreshOptionStyle(QWidget* card, bool selected);
    QString AnswerSummary(const QString& status, const QJsonObject& answer) const;

    bool m_pending = true;
    bool m_allowMultiple = false;
    QButtonGroup* m_group = nullptr;
    QVector<QAbstractButton*> m_choices;
    QPlainTextEdit* m_otherText = nullptr;
    QPushButton* m_continue = nullptr;
};
