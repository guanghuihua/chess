#include <QCoreApplication>
#include <QJsonObject>

#include "json_object_extractor.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QJsonObject object;
    QString error;
    if (!JsonObjectExtractor::parse(
            QStringLiteral("{\"overview\":\"ok\"}"), &object, &error)
        || object.value("overview").toString() != "ok") {
        return 1;
    }
    if (!JsonObjectExtractor::parse(
            QStringLiteral("下面是结果：\n```json\n{\"overview\":\"含有 { 括号 }\",\"n\":1}\n```\n谢谢"),
            &object, &error)
        || object.value("n").toInt() != 1) {
        return 2;
    }
    if (JsonObjectExtractor::parse(QStringLiteral("不是 JSON"), &object, &error)
        || error.isEmpty()) {
        return 3;
    }
    if (JsonObjectExtractor::parse(QStringLiteral("{\"broken\":"), &object, &error)
        || error.isEmpty()) {
        return 4;
    }
    return 0;
}
