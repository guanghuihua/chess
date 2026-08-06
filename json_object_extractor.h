#ifndef JSON_OBJECT_EXTRACTOR_H
#define JSON_OBJECT_EXTRACTOR_H

#include <QString>

class QJsonObject;

namespace JsonObjectExtractor {

bool parse(const QString &text, QJsonObject *object,
           QString *errorMessage = nullptr);

}

#endif // JSON_OBJECT_EXTRACTOR_H
