#include "deepseek_coach.h"

#include "credential_store.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

namespace {
const char modelName[] = "deepseek-v4-flash";
const char endpoint[] = "https://api.deepseek.com/chat/completions";
}

DeepSeekCoach::DeepSeekCoach(QObject *parent)
    : QObject(parent)
    , api_key_(QProcessEnvironment::systemEnvironment().value("DEEPSEEK_API_KEY"))
{
    if (api_key_.isEmpty()) {
        api_key_ = CredentialStore::readDeepSeekApiKey();
    }
    QTimer::singleShot(0, this, [this] {
        if (api_key_.isEmpty()) {
            emit statusChanged(QString::fromUtf8(
                u8"DeepSeek 未启用：请设置 DEEPSEEK_API_KEY 环境变量"), false);
        } else {
            emit statusChanged(QString::fromUtf8(u8"DeepSeek AI 教练已启用"), true);
        }
    });
}

bool DeepSeekCoach::isConfigured() const
{
    return !api_key_.isEmpty();
}

bool DeepSeekCoach::saveApiKey(const QString &apiKey, QString *errorMessage)
{
    const QString normalized = apiKey.trimmed();
    if (!CredentialStore::writeDeepSeekApiKey(normalized, errorMessage)) {
        return false;
    }
    api_key_ = normalized;
    emit statusChanged(QString::fromUtf8(u8"DeepSeek 密钥已安全保存，正在测试连接……"), true);
    return true;
}

bool DeepSeekCoach::removeApiKey(QString *errorMessage)
{
    if (!CredentialStore::removeDeepSeekApiKey(errorMessage)) {
        return false;
    }
    api_key_.clear();
    emit statusChanged(QString::fromUtf8(u8"DeepSeek 密钥已删除"), false);
    return true;
}

void DeepSeekCoach::testConnection()
{
    if (api_key_.isEmpty()) {
        const QString message = QString::fromUtf8(u8"尚未配置 DeepSeek API Key");
        emit statusChanged(message, false);
        emit connectionTested(false, message);
        return;
    }

    QNetworkRequest request(QUrl("https://api.deepseek.com/models"));
    request.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    request.setTransferTimeout(20000);
    QNetworkReply *reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray body = reply->readAll();
        const bool networkOk = reply->error() == QNetworkReply::NoError;
        const QString networkError = reply->errorString();
        reply->deleteLater();

        bool modelFound = false;
        if (networkOk) {
            const QJsonArray models = QJsonDocument::fromJson(body)
                                          .object().value("data").toArray();
            for (const QJsonValue &value : models) {
                if (value.toObject().value("id").toString() == QString::fromLatin1(modelName)) {
                    modelFound = true;
                    break;
                }
            }
        }

        const bool success = networkOk && modelFound;
        const QString message = success
            ? QString::fromUtf8(u8"连接成功：DeepSeek V4 Flash 可用")
            : (networkOk
                   ? QString::fromUtf8(u8"连接成功，但账号暂时不可用 DeepSeek V4 Flash")
                   : QString::fromUtf8(u8"连接失败：") + networkError);
        emit statusChanged(message, success);
        emit connectionTested(success, message);
    });
}

void DeepSeekCoach::requestCoaching(
    const PikafishAnalyzer::AnalysisResult &analysis,
    const GameDatabase::TrainingStats &stats)
{
    if (api_key_.isEmpty()) {
        return;
    }
    requests_.enqueue(Request{analysis, stats});
    processNext();
}

void DeepSeekCoach::processNext()
{
    if (busy_ || requests_.isEmpty() || api_key_.isEmpty()) {
        return;
    }

    busy_ = true;
    const Request requestData = requests_.dequeue();
    QNetworkRequest networkRequest(QUrl(QString::fromLatin1(endpoint)));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    networkRequest.setTransferTimeout(45000);

    emit statusChanged(QString::fromUtf8(u8"DeepSeek 正在生成第 %1 步的个性化讲解……")
                           .arg(requestData.analysis.ply), true);
    QNetworkReply *reply = network_.post(networkRequest, makeRequestBody(requestData));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestData] {
        handleReply(reply, requestData);
    });
}

void DeepSeekCoach::handleReply(QNetworkReply *reply, const Request &request)
{
    const QByteArray responseBody = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    busy_ = false;

    if (networkError != QNetworkReply::NoError) {
        emit statusChanged(QString::fromUtf8(u8"DeepSeek 请求失败：") + networkErrorText, false);
        processNext();
        return;
    }

    CoachingResult result;
    QString error;
    if (!parseCoachingContent(responseBody, request, &result, &error)) {
        emit statusChanged(QString::fromUtf8(u8"DeepSeek 返回内容无法解析：") + error, false);
        processNext();
        return;
    }

    emit coachingReady(result);
    emit statusChanged(QString::fromUtf8(u8"DeepSeek AI 教练已就绪"), true);
    processNext();
}

QByteArray DeepSeekCoach::makeRequestBody(const Request &request)
{
    const auto &analysis = request.analysis;
    const auto &stats = request.stats;

    const QString systemPrompt = QString::fromUtf8(
        u8"你是一名严谨、友善的中国象棋教练。Pikafish 引擎负责棋局计算，你只负责根据给定证据进行教学解释。"
        u8"不得否定或修改引擎给出的最佳走法和评分，不得编造未提供的变化。"
        u8"请关注学习者的思考习惯，而不是只批评结果。必须输出一个 JSON 对象，且只包含以下四个字符串字段："
        u8"diagnosis（本步诊断，2到3句）、evidence（引用输入证据）、training_task（一个可执行训练任务）、"
        u8"reflection_question（一个引导复盘的问题）。使用简洁中文。"
        u8"示例 JSON：{\"diagnosis\":\"...\",\"evidence\":\"...\","
        u8"\"training_task\":\"...\",\"reflection_question\":\"...\"}");

    const QString userPrompt = QString::fromUtf8(
        u8"请根据以下结构化证据输出 JSON 教练建议：\n"
        u8"步数：%1\n实际走法：%2（%3）\n引擎推荐：%4（%5）\n"
        u8"最佳评分：%6\n实际评分：%7\n局面损失：%8\n错误等级：%9\n"
        u8"思考时间：%10 毫秒\n推荐变化：%11\n走棋前局面编码：%12\n"
        u8"长期统计：累计对局 %13，已分析红方走法 %14，平均损失 %15，严重失误 %16。")
        .arg(analysis.ply)
        .arg(analysis.actualNotation, analysis.actualMove,
             analysis.bestNotation, analysis.bestMove)
        .arg(analysis.bestScore)
        .arg(analysis.actualScore)
        .arg(analysis.scoreLoss)
        .arg(analysis.category)
        .arg(analysis.thinkingTimeMs)
        .arg(analysis.principalVariation)
        .arg(analysis.boardBefore)
        .arg(stats.games)
        .arg(stats.analyzedMoves)
        .arg(stats.averageLoss, 0, 'f', 1)
        .arg(stats.blunders);

    QJsonObject body;
    body["model"] = QString::fromLatin1(modelName);
    body["stream"] = false;
    body["max_tokens"] = 700;
    body["temperature"] = 0.3;
    body["thinking"] = QJsonObject{{"type", "disabled"}};
    body["response_format"] = QJsonObject{{"type", "json_object"}};
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", userPrompt}}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

bool DeepSeekCoach::parseCoachingContent(const QByteArray &body,
                                         const Request &request,
                                         CoachingResult *result,
                                         QString *errorMessage)
{
    QJsonParseError outerError;
    const QJsonDocument outerDocument = QJsonDocument::fromJson(body, &outerError);
    if (outerError.error != QJsonParseError::NoError || !outerDocument.isObject()) {
        *errorMessage = outerError.errorString();
        return false;
    }

    const QJsonObject outer = outerDocument.object();
    if (outer.contains("error")) {
        *errorMessage = outer.value("error").toObject().value("message").toString();
        return false;
    }
    const QJsonArray choices = outer.value("choices").toArray();
    if (choices.isEmpty()) {
        *errorMessage = QString::fromUtf8(u8"响应中没有 choices");
        return false;
    }
    const QString content = choices[0].toObject()
                                .value("message").toObject()
                                .value("content").toString();
    if (content.trimmed().isEmpty()) {
        *errorMessage = QString::fromUtf8(u8"模型返回了空内容");
        return false;
    }

    QJsonParseError contentError;
    const QJsonDocument contentDocument = QJsonDocument::fromJson(content.toUtf8(), &contentError);
    if (contentError.error != QJsonParseError::NoError || !contentDocument.isObject()) {
        *errorMessage = contentError.errorString();
        return false;
    }
    const QJsonObject coaching = contentDocument.object();
    const QString diagnosis = coaching.value("diagnosis").toString().trimmed();
    const QString evidence = coaching.value("evidence").toString().trimmed();
    const QString trainingTask = coaching.value("training_task").toString().trimmed();
    const QString reflectionQuestion = coaching.value("reflection_question").toString().trimmed();
    if (diagnosis.isEmpty() || evidence.isEmpty() ||
        trainingTask.isEmpty() || reflectionQuestion.isEmpty()) {
        *errorMessage = QString::fromUtf8(u8"JSON 缺少必要字段");
        return false;
    }

    result->gameId = request.analysis.gameId;
    result->ply = request.analysis.ply;
    result->actualMove = request.analysis.actualMove;
    result->model = QString::fromLatin1(modelName);
    result->diagnosis = diagnosis;
    result->evidence = evidence;
    result->trainingTask = trainingTask;
    result->reflectionQuestion = reflectionQuestion;
    return true;
}
