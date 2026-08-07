#include "credential_store.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
#ifdef Q_OS_WIN
const wchar_t credentialTarget[] = L"GuanghuiEducationLab.XiangqiTraining.PackyApiKey";

QString windowsErrorMessage(DWORD errorCode)
{
    wchar_t *messageBuffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0,
        reinterpret_cast<wchar_t *>(&messageBuffer), 0, nullptr);
    const QString message = length > 0
                                ? QString::fromWCharArray(messageBuffer, static_cast<int>(length)).trimmed()
                                : QString::fromUtf8(u8"Windows 错误 %1").arg(errorCode);
    if (messageBuffer) {
        LocalFree(messageBuffer);
    }
    return message;
}
#endif
}

QString CredentialStore::readPackyApiKey(QString *errorMessage)
{
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(credentialTarget, CRED_TYPE_GENERIC, 0, &credential)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND && errorMessage) {
            *errorMessage = windowsErrorMessage(error);
        }
        return QString();
    }

    QByteArray value(
        reinterpret_cast<const char *>(credential->CredentialBlob),
        static_cast<int>(credential->CredentialBlobSize));
    const QString apiKey = QString::fromUtf8(value);
    SecureZeroMemory(value.data(), static_cast<SIZE_T>(value.size()));
    CredFree(credential);
    return apiKey;
#else
    if (errorMessage) {
        *errorMessage = QString::fromUtf8(u8"当前系统不支持 Windows Credential Manager");
    }
    return QString();
#endif
}

bool CredentialStore::writePackyApiKey(const QString &apiKey,
                                       QString *errorMessage)
{
#ifdef Q_OS_WIN
    QByteArray value = apiKey.trimmed().toUtf8();
    if (value.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"API Key 不能为空");
        }
        return false;
    }
    if (value.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"API Key 长度超过系统限制");
        }
        return false;
    }

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t *>(credentialTarget);
    credential.CredentialBlobSize = static_cast<DWORD>(value.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(value.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<wchar_t *>(L"Packy API");

    const bool written = CredWriteW(&credential, 0) != FALSE;
    if (!written && errorMessage) {
        *errorMessage = windowsErrorMessage(GetLastError());
    }
    SecureZeroMemory(value.data(), static_cast<SIZE_T>(value.size()));
    return written;
#else
    Q_UNUSED(apiKey);
    if (errorMessage) {
        *errorMessage = QString::fromUtf8(u8"当前系统不支持 Windows Credential Manager");
    }
    return false;
#endif
}

bool CredentialStore::removePackyApiKey(QString *errorMessage)
{
#ifdef Q_OS_WIN
    if (CredDeleteW(credentialTarget, CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_NOT_FOUND) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = windowsErrorMessage(error);
    }
    return false;
#else
    if (errorMessage) {
        *errorMessage = QString::fromUtf8(u8"当前系统不支持 Windows Credential Manager");
    }
    return false;
#endif
}
