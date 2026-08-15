#include "ai/SecureCredentialStore.hpp"

#include "mcp/PyBind.hpp"

namespace py = pybind11;

namespace ai {
namespace {

py::module_ CredentialModule() {
    return py::module_::import("tools.ai.credential_store");
}

QString PythonError(const py::error_already_set& error) {
    return QString::fromUtf8(error.what());
}

} // namespace

bool SecureCredentialStore::IsAvailable(QString* reason) {
    py::gil_scoped_acquire gil;
    try {
        const py::tuple result = CredentialModule().attr("is_available")().cast<py::tuple>();
        const bool available = result[0].cast<bool>();
        if (reason) *reason = QString::fromStdString(result[1].cast<std::string>());
        return available;
    } catch (const py::error_already_set& error) {
        if (reason) *reason = PythonError(error);
        return false;
    }
}

bool SecureCredentialStore::Store(const QString& account, const QByteArray& secret,
                                  QString* error) {
    py::gil_scoped_acquire gil;
    try {
        CredentialModule().attr("store")(
            account.toStdString(), py::bytes(secret.constData(), secret.size()));
        if (error) error->clear();
        return true;
    } catch (const py::error_already_set& pythonError) {
        if (error) *error = PythonError(pythonError);
        return false;
    }
}

QByteArray SecureCredentialStore::Load(const QString& account, QString* error) {
    py::gil_scoped_acquire gil;
    try {
        const py::object value = CredentialModule().attr("load")(account.toStdString());
        if (value.is_none()) {
            if (error) error->clear();
            return {};
        }
        const std::string bytes = value.cast<std::string>();
        if (error) error->clear();
        return QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
    } catch (const py::error_already_set& pythonError) {
        if (error) *error = PythonError(pythonError);
        return {};
    }
}

bool SecureCredentialStore::Remove(const QString& account, QString* error) {
    py::gil_scoped_acquire gil;
    try {
        CredentialModule().attr("remove")(account.toStdString());
        if (error) error->clear();
        return true;
    } catch (const py::error_already_set& pythonError) {
        if (error) *error = PythonError(pythonError);
        return false;
    }
}

} // namespace ai
