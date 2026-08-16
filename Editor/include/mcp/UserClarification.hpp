#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcp {

struct ClarificationOption {
    QString id;
    QString label;
    QString description;
};

struct ClarificationRequest {
    QString title;
    QString question;
    QString context;
    QVector<ClarificationOption> options;
    bool allowMultiple = false;

    // Non-empty for a request that authorizes a particular engine operation.
    // Stored inside the editor and never supplied by the model on creation.
    QString scope;
};

// Owns clarification requests and their answers. Create() emits an inline transcript
// presentation request and returns immediately; the stdio MCP wrapper polls Status()
// while the user answers inside the AI Assistant thread. No nested event loop is used.
class UserClarificationService final : public QObject {
    Q_OBJECT
public:
    static UserClarificationService* Get();

    QString Create(ClarificationRequest request);
    bool Answer(const QString& requestId, const QStringList& selectedIds,
                const QString& otherText, QString* error = nullptr);
    bool Cancel(const QString& requestId);
    QJsonObject Status(const QString& requestId) const;

    // Consume a user answer as authorization for one scoped engine action. A model
    // cannot forge this with a boolean: the request id, scope, answer, and one-shot
    // consumed bit all live in this editor process.
    bool Consume(const QString& requestId, const QString& expectedScope,
                 const QString& requiredSelection, QJsonObject* answer,
                 QString* error);

Q_SIGNALS:
    void ClarificationRequested(const QString& requestId,
                                const QJsonObject& presentation);
    void ClarificationResolved(const QString& requestId,
                               const QJsonObject& result);

private:
    explicit UserClarificationService(QObject* parent = nullptr);
    struct Record;
    void PruneFinished();

    QHash<QString, Record*> m_records;
};

} // namespace mcp
