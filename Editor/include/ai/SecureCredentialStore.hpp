#pragma once

#include <QByteArray>
#include <QString>

namespace ai {

// Pybind11 bridge to tools/ai/credential_store.py. Python owns all native vault
// behavior; Qt only passes byte strings in and out while the GIL is held.
// Secrets never enter QSettings, the repository, command-line arguments, or a
// .env file, and the Python implementation deliberately has no plaintext fallback.
class SecureCredentialStore {
public:
    static bool IsAvailable(QString* reason = nullptr);
    static bool Store(const QString& account, const QByteArray& secret, QString* error = nullptr);
    static QByteArray Load(const QString& account, QString* error = nullptr);
    static bool Remove(const QString& account, QString* error = nullptr);
};

} // namespace ai
