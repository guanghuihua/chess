#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H

#include <QString>

class CredentialStore
{
public:
    static QString readDeepSeekApiKey(QString *errorMessage = nullptr);
    static bool writeDeepSeekApiKey(const QString &apiKey,
                                    QString *errorMessage = nullptr);
    static bool removeDeepSeekApiKey(QString *errorMessage = nullptr);
};

#endif // CREDENTIAL_STORE_H
