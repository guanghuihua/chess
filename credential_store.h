#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H

#include <QString>

class CredentialStore
{
public:
    static QString readPackyApiKey(QString *errorMessage = nullptr);
    static bool writePackyApiKey(const QString &apiKey,
                                 QString *errorMessage = nullptr);
    static bool removePackyApiKey(QString *errorMessage = nullptr);
};

#endif // CREDENTIAL_STORE_H
