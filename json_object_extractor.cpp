#include "json_object_extractor.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
QString firstJsonObject(const QString &text)
{
    const int start = text.indexOf('{');
    if (start < 0) return {};

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int index = start; index < text.size(); ++index) {
        const QChar character = text[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0) return text.mid(start, index - start + 1);
        }
    }
    return {};
}
}

bool JsonObjectExtractor::parse(const QString &text, QJsonObject *object,
                                QString *errorMessage)
{
    if (!object) {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"JSON 输出对象不能为空");
        return false;
    }
    const QString candidate = firstJsonObject(text.trimmed());
    if (candidate.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(
                u8"模型回复中没有找到完整 JSON 对象。回复开头：%1")
                .arg(text.trimmed().left(80));
        }
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(candidate.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"JSON 解析失败：%1（位置 %2）")
                .arg(error.errorString()).arg(error.offset);
        }
        return false;
    }
    *object = document.object();
    return true;
}
