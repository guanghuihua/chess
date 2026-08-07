#include "deepseek_coach.h"

#include "credential_store.h"
#include "json_object_extractor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

namespace {
const char deepSeekEndpoint[] = "https://api.deepseek.com/chat/completions";
const char deepSeekModel[] = "deepseek-chat";
}

DeepSeekCoach::DeepSeekCoach(QObject *parent)
    : QObject(parent)
{
    const QString environmentKey = QProcessEnvironment::systemEnvironment()
                                       .value("DEEPSEEK_API_KEY").trimmed();
    if (!environmentKey.isEmpty()) {
        api_key_ = environmentKey;
    }
    if (api_key_.isEmpty()) {
        api_key_ = CredentialStore::readDeepSeekApiKey();
    }
    QTimer::singleShot(0, this, [this] {
        if (api_key_.isEmpty()) {
            emit statusChanged(QString::fromUtf8(
                u8"AI 未启用：请配置 DeepSeek API Key，或提供 DEEPSEEK_API_KEY"), false);
        } else {
            emit statusChanged(QString::fromUtf8(
                u8"DeepSeek 已启用 · 用于讲解、复盘与训练出题"), true);
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

    const QUrl modelsUrl(QStringLiteral("https://api.deepseek.com/models"));
    QNetworkRequest request(modelsUrl);
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
                const QString id = value.toObject().value("id").toString();
                modelFound = modelFound || id == activeFastModel();
            }
        }

        const bool success = networkOk && modelFound;
        const QString message = success
            ? QString::fromUtf8(u8"连接成功：DeepSeek Chat 可用")
            : (networkOk
                    ? QString::fromUtf8(u8"连接成功，但账号暂时不可用 DeepSeek Chat")
                   : QString::fromUtf8(u8"连接失败：") + networkError);
        emit statusChanged(message, success);
        emit connectionTested(success, message);
    });
}

void DeepSeekCoach::requestCoaching(
    const PikafishAnalyzer::AnalysisResult &analysis,
    const GameDatabase::TrainingStats &stats,
    const QString &gameContext)
{
    if (api_key_.isEmpty()) {
        return;
    }
    requests_.enqueue(Request{analysis, stats, gameContext});
    processNext();
}

void DeepSeekCoach::requestGameReview(
    const GameDatabase::GameReviewContext &context,
    const GameDatabase::TrainingStats &stats)
{
    if (api_key_.isEmpty() || context.gameId < 0) {
        return;
    }
    game_review_requests_.enqueue(GameReviewRequest{context, stats});
    processNext();
}

void DeepSeekCoach::requestChat(const QString &requestId,
                                const QString &evidenceContext,
                                const QString &conversationHistory,
                                const QString &question)
{
    if (api_key_.isEmpty() || requestId.isEmpty() || question.trimmed().isEmpty()) {
        emit chatReplyReady(requestId, QString(),
                            QString::fromUtf8(u8"DeepSeek 尚未配置或问题为空"));
        return;
    }
    chat_requests_.enqueue(ChatRequest{requestId, evidenceContext,
                                       conversationHistory, question.trimmed()});
    processNext();
}

void DeepSeekCoach::requestGeneratedExercise(const QString &requestId,
                                             const QString &profileContext)
{
    if (api_key_.isEmpty() || requestId.isEmpty() || profileContext.trimmed().isEmpty()) {
        ExerciseDraft draft;
        draft.requestId = requestId;
        emit exerciseDraftReady(draft, QString::fromUtf8(u8"DeepSeek 尚未配置，或用户画像证据不足。"));
        return;
    }
    exercise_requests_.enqueue(ExerciseRequest{requestId, profileContext.left(10000)});
    processNext();
}

void DeepSeekCoach::processNext()
{
    if (busy_ || api_key_.isEmpty()) {
        return;
    }
    if (requests_.isEmpty()) {
        processNextChat();
        return;
    }

    busy_ = true;
    const Request requestData = requests_.dequeue();
    QNetworkRequest networkRequest(activeEndpoint());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    networkRequest.setTransferTimeout(90000);

    emit statusChanged(QString::fromUtf8(u8"AI 正在结合完整棋谱生成第 %1 步讲解……")
                           .arg(requestData.analysis.ply), true);
    QNetworkReply *reply = network_.post(
        networkRequest, providerRequestBody(makeRequestBody(requestData), false));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestData] {
        handleReply(reply, requestData);
    });
}

void DeepSeekCoach::processNextChat()
{
    if (busy_ || api_key_.isEmpty()) return;
    if (chat_requests_.isEmpty()) {
        processNextExercise();
        return;
    }

    busy_ = true;
    const ChatRequest requestData = chat_requests_.dequeue();
    QNetworkRequest networkRequest(activeEndpoint());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    networkRequest.setTransferTimeout(60000);
    emit statusChanged(QString::fromUtf8(u8"AI 教练正在回答你的追问……"), true);
    QNetworkReply *reply = network_.post(
        networkRequest, providerRequestBody(makeChatRequestBody(requestData), false));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestData] {
        handleChatReply(reply, requestData);
    });
}

void DeepSeekCoach::processNextExercise()
{
    if (busy_ || api_key_.isEmpty()) return;
    if (exercise_requests_.isEmpty()) {
        processNextGameReview();
        return;
    }

    busy_ = true;
    const ExerciseRequest requestData = exercise_requests_.dequeue();
    QNetworkRequest networkRequest(activeEndpoint());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    networkRequest.setTransferTimeout(90000);
    emit statusChanged(QString::fromUtf8(u8"AI 正在根据用户画像设计新的专项题……"), true);
    QNetworkReply *reply = network_.post(
        networkRequest, providerRequestBody(makeExerciseRequestBody(requestData), false));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestData] {
        handleExerciseReply(reply, requestData);
    });
}

void DeepSeekCoach::processNextGameReview()
{
    if (busy_ || game_review_requests_.isEmpty() || api_key_.isEmpty()) {
        return;
    }

    busy_ = true;
    const GameReviewRequest requestData = game_review_requests_.dequeue();
    QNetworkRequest networkRequest(activeEndpoint());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", "Bearer " + api_key_.toUtf8());
    networkRequest.setTransferTimeout(120000);

    emit statusChanged(QString::fromUtf8(u8"AI 正在生成第 %1 盘的整盘复盘……")
                           .arg(requestData.context.gameId), true);
    QNetworkReply *reply = network_.post(
        networkRequest,
        providerRequestBody(makeGameReviewRequestBody(requestData), true));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestData] {
        handleGameReviewReply(reply, requestData);
    });
}

void DeepSeekCoach::handleReply(QNetworkReply *reply, const Request &request)
{
    const QByteArray responseBody = normalizedResponseBody(reply->readAll());
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    busy_ = false;

    if (networkError != QNetworkReply::NoError) {
        emit statusChanged(QString::fromUtf8(u8"AI 请求失败：") + networkErrorText, false);
        processNext();
        return;
    }

    CoachingResult result;
    QString error;
    if (!parseCoachingContent(responseBody, request, &result, &error)) {
        emit statusChanged(QString::fromUtf8(u8"AI 返回内容无法解析：") + error, false);
        processNext();
        return;
    }

    result.model = activeFastModel();
    emit coachingReady(result);
    emit statusChanged(QString::fromUtf8(u8"AI 教练建议已就绪"), true);
    processNext();
}

void DeepSeekCoach::handleGameReviewReply(QNetworkReply *reply,
                                          const GameReviewRequest &request)
{
    const QByteArray responseBody = normalizedResponseBody(reply->readAll());
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    busy_ = false;

    if (networkError != QNetworkReply::NoError) {
        const QString reason = httpStatus > 0
            ? QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(networkErrorText)
            : networkErrorText;
        retryOrFallbackGameReview(request, reason);
        return;
    }

    GameReviewResult result;
    QString error;
    if (!parseGameReviewContent(responseBody, request, &result, &error)) {
        retryOrFallbackGameReview(request, error);
        return;
    }

    result.model = activeReviewModel();
    emit gameReviewReady(result);
    emit statusChanged(QString::fromUtf8(u8"AI 整盘复盘已完成"), true);
    processNext();
}

void DeepSeekCoach::retryOrFallbackGameReview(
    const GameReviewRequest &request, const QString &reason)
{
    if (request.attempt == 0) {
        GameReviewRequest retry = request;
        retry.attempt = 1;
        game_review_requests_.prepend(retry);
        emit statusChanged(
            QString::fromUtf8(u8"整盘复盘返回异常，正在自动重试：") + reason,
            true);
        processNext();
        return;
    }

    const auto &context = request.context;
    GameReviewResult fallback;
    fallback.gameId = context.gameId;
    fallback.userId = context.userId;
    fallback.model = QStringLiteral("local-engine-coach");
    fallback.overview = QString::fromUtf8(
        u8"远程 AI 两次未能生成有效内容，本复盘已自动改用 Pikafish 引擎数据。"
        u8"全局共走 %1 步，红方已有 %2 步完成分析，平均损失为 %3。")
        .arg(context.totalMoves)
        .arg(context.analyzedMoves)
        .arg(context.averageLoss, 0, 'f', 1);
    fallback.turningPoints = context.keyMoments.trimmed().isEmpty()
        ? QString::fromUtf8(u8"现有引擎数据不足以确定关键转折点。")
        : context.keyMoments;
    fallback.strengths = context.blunders == 0
        ? QString::fromUtf8(u8"本局没有被引擎判定为严重失误的红方着法。")
        : QString::fromUtf8(u8"你完成了整盘对局，并留下了可用于针对训练的真实决策数据。");
    fallback.recurringPattern = context.undoSummary.trimmed().isEmpty()
        ? QString::fromUtf8(u8"当前证据不足，暂不判断重复性思维模式。")
        : context.undoSummary;
    fallback.trainingPlan = context.blunders > 0
        ? QString::fromUtf8(u8"1. 重做本局损失最大的局面；2. 落子前检查将军、吃子和直接威胁；3. 一周后再次测试同类局面。")
        : QString::fromUtf8(u8"1. 复查关键转折点；2. 比较实际着与引擎推荐着；3. 写下当时考虑过的候选着。");
    fallback.reflectionQuestion = QString::fromUtf8(
        u8"本局哪一步最能反映你的思考习惯？如果重新选择，你会先检查什么？");

    emit gameReviewReady(fallback);
    emit statusChanged(
        QString::fromUtf8(u8"AI 整盘复盘失败，已自动生成本地引擎复盘：")
            + reason,
        false);
    processNext();
}

void DeepSeekCoach::handleChatReply(QNetworkReply *reply,
                                    const ChatRequest &request)
{
    const QByteArray body = normalizedResponseBody(reply->readAll());
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    busy_ = false;

    QString error;
    QString answer;
    if (networkError != QNetworkReply::NoError) {
        error = networkErrorText;
    } else {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QJsonObject object = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = parseError.errorString();
        } else if (object.contains("error")) {
            error = object.value("error").toObject().value("message").toString();
        } else {
            const QJsonArray choices = object.value("choices").toArray();
            if (!choices.isEmpty()) {
                answer = choices[0].toObject().value("message").toObject()
                             .value("content").toString().trimmed();
            }
            if (answer.isEmpty()) error = QString::fromUtf8(u8"模型返回了空回答");
        }
    }

    emit chatReplyReady(request.requestId, answer, error);
    emit statusChanged(error.isEmpty()
                           ? QString::fromUtf8(u8"AI 教练回答已完成")
                           : QString::fromUtf8(u8"AI 教练回答失败：") + error,
                       error.isEmpty());
    processNext();
}

void DeepSeekCoach::handleExerciseReply(QNetworkReply *reply,
                                        const ExerciseRequest &request)
{
    const QByteArray body = normalizedResponseBody(reply->readAll());
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    busy_ = false;

    ExerciseDraft draft;
    draft.requestId = request.requestId;
    QString error;
    QJsonParseError outerError;
    const QJsonDocument outerDocument = QJsonDocument::fromJson(body, &outerError);
    if (networkError != QNetworkReply::NoError) {
        error = networkErrorText;
    } else if (outerError.error != QJsonParseError::NoError || !outerDocument.isObject()) {
        error = outerError.errorString();
    } else if (outerDocument.object().contains("error")) {
        error = outerDocument.object().value("error").toObject().value("message").toString();
    } else {
        const QJsonArray choices = outerDocument.object().value("choices").toArray();
        const QString content = choices.isEmpty() ? QString()
            : choices.at(0).toObject().value("message").toObject().value("content").toString();
        QJsonObject proposal;
        if (content.trimmed().isEmpty()
            || !JsonObjectExtractor::parse(content, &proposal, &error)) {
            if (error.isEmpty()) error = QString::fromUtf8(u8"AI 返回了空题目草案。");
        } else {
            draft.board = proposal.value("board").toString().trimmed();
            draft.theme = proposal.value("theme").toString().trimmed();
            draft.diagnosisTag = proposal.value("diagnosis_tag").toString().trimmed();
            draft.learningGoal = proposal.value("learning_goal").toString().trimmed();
            draft.hint = proposal.value("hint").toString().trimmed();
            if (draft.board.isEmpty() || draft.theme.isEmpty() || draft.diagnosisTag.isEmpty()
                || draft.learningGoal.isEmpty() || draft.hint.isEmpty()) {
                error = QString::fromUtf8(u8"AI 题目草案字段不完整，已拒绝入题库。");
            }
        }
    }
    emit exerciseDraftReady(draft, error);
    emit statusChanged(error.isEmpty()
                           ? QString::fromUtf8(u8"AI 题目草案已生成，正在交给 Pikafish 验题")
                           : QString::fromUtf8(u8"AI 出题失败：") + error,
                       error.isEmpty());
    processNext();
}

QByteArray DeepSeekCoach::makeChatRequestBody(const ChatRequest &request)
{
    const QString systemPrompt = QString::fromUtf8(
        u8"你是一名直接、严谨的中国象棋私人教练。回答学习者针对某一步或整盘棋的追问。"
        u8"Pikafish 引擎证据优先于语言推测；不要修改评分、最佳着或推荐变化，也不要编造未提供的计算。"
        u8"总长度不超过 220 个汉字，只写四项：1. 错在哪；2. 对手如何惩罚；3. 推荐着解决什么；4. 下次检查什么。"
        u8"每项最多两句。禁止寒暄、鼓励、复述用户问题、重复整段评分、空泛地说‘加强计算’或‘注意局面’。"
        u8"只能解释输入中已经给出的中文棋谱、验证变化和评分差异。不得根据记忆、坐标棋谱或棋盘编码推测棋子位置、"
        u8"行棋路线、攻击、保护、将军、吃子或交换关系，也不得自行计算新变化。"
        u8"若给出的变化不足以证明具体战术原因，明确写‘现有引擎变化只确认评分恶化，请在棋盘回放中核对具体子力关系’。"
        u8"禁止输出 a0-i9、f5-c3 等坐标着法；所有走法必须使用中文棋谱，例如车九进一。");
    const QString userPrompt = QString::fromUtf8(
        u8"【当前棋局证据】\n%1\n\n【此前对话】\n%2\n\n【学习者的新问题】\n%3")
        .arg(request.evidenceContext.left(12000),
             request.conversationHistory.right(6000), request.question);
    QJsonObject body;
    body["model"] = QString::fromLatin1(deepSeekModel);
    body["stream"] = false;
    body["max_tokens"] = 520;
    body["temperature"] = 0.2;
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", userPrompt}}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray DeepSeekCoach::makeExerciseRequestBody(const ExerciseRequest &request)
{
    const QString systemPrompt = QString::fromUtf8(
        u8"你是中国象棋个性化训练出题人。根据用户画像设计一题新的红方走棋题；题面不得复制任何历史错局。"
        u8"只输出一个 JSON 对象，字段必须为 board、theme、diagnosis_tag、learning_goal、hint。"
        u8"board 是 10 行、每行 9 格、以 / 分隔的局面编码：红方用大写 K A E H R C S，黑方用小写 k a e h r c s，空格用 .；"
        u8"红方走。双方将帅必须存在。不要输出答案、坐标着法、棋谱或 Markdown。"
        u8"题目必须围绕画像中的一个弱项，要求有可计算的将军、吃子、威胁、兑子或残局技术目标；"
        u8"theme、learning_goal、hint 均使用简洁中文，hint 只能给思考方向而不能泄露着法。"
        u8"局面将由规则程序和 Pikafish 独立验证；不确定合法性时宁可选择简单、素材少、局面清晰的局面。");
    QJsonObject body;
    body["model"] = QString::fromLatin1(deepSeekModel);
    body["stream"] = false;
    body["max_tokens"] = 650;
    body["temperature"] = 0.65;
    body["response_format"] = QJsonObject{{"type", "json_object"}};
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", request.profileContext}}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString DeepSeekCoach::activeFastModel() const
{
    return QString::fromLatin1(deepSeekModel);
}

QString DeepSeekCoach::activeReviewModel() const
{
    return QString::fromLatin1(deepSeekModel);
}

QUrl DeepSeekCoach::activeEndpoint() const
{
    return QUrl(QString::fromLatin1(deepSeekEndpoint));
}

QByteArray DeepSeekCoach::providerRequestBody(
    const QByteArray &chatCompletionsBody, bool wholeGame) const
{
    QJsonObject body = QJsonDocument::fromJson(chatCompletionsBody).object();
    body["model"] = wholeGame ? activeReviewModel() : activeFastModel();
    body["stream"] = false;
    body.remove("thinking");
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray DeepSeekCoach::normalizedResponseBody(const QByteArray &body) const
{
    QString combined;
    QString providerError;
    const auto consumeEvent = [&combined, &providerError](const QJsonObject &event) {
        const QString type = event.value("type").toString();
        if (type == "error" || type == "response.failed") {
            const QJsonObject error = event.value("error").toObject();
            providerError = error.value("message").toString();
            if (providerError.isEmpty()) providerError = event.value("message").toString();
            return;
        }
        if (type == "response.output_text.delta") {
            combined += event.value("delta").toString();
            return;
        }
        if (type == "response.output_text.done") {
            const QString text = event.value("text").toString();
            if (!text.isEmpty()) combined = text;
            return;
        }
        const QString outputText = event.value("output_text").toString();
        if (!outputText.isEmpty()) combined += outputText;
        for (const QJsonValue &item : event.value("output").toArray()) {
            for (const QJsonValue &content : item.toObject().value("content").toArray()) {
                const QString text = content.toObject().value("text").toString();
                if (!text.isEmpty()) combined += text;
            }
        }
    };
    const QByteArray trimmed = body.trimmed();
    if (trimmed.startsWith('{')) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(trimmed, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();
            if (object.contains("choices")) return trimmed;
            consumeEvent(object);
        }
    }
    for (QByteArray line : body.split('\n')) {
        line = line.trimmed();
        if (!line.startsWith("data:")) continue;
        const QByteArray json = line.mid(5).trimmed();
        if (json == "[DONE]") continue;
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            consumeEvent(document.object());
        }
    }
    if (combined.trimmed().isEmpty() && !providerError.isEmpty()) {
        return QJsonDocument(QJsonObject{{"error", QJsonObject{{"message", providerError}}}})
            .toJson(QJsonDocument::Compact);
    }
    QJsonObject normalized;
    normalized["choices"] = QJsonArray{QJsonObject{
        {"finish_reason", combined.trimmed().isEmpty() ? "empty_stream" : "stop"},
        {"message", QJsonObject{{"content", combined}}}
    }};
    return QJsonDocument(normalized).toJson(QJsonDocument::Compact);
}

QByteArray DeepSeekCoach::makeRequestBody(const Request &request)
{
    const auto &analysis = request.analysis;
    const auto &stats = request.stats;

    const QString systemPrompt = QString::fromUtf8(
        u8"你是一名直接、严谨的中国象棋教练。Pikafish 引擎负责棋局计算，你只负责根据给定证据解释决策错误。"
        u8"不得否定或修改引擎给出的最佳走法和评分，不得编造未提供的变化。"
        u8"禁止寒暄、鼓励、抽象评价性格，以及‘加强计算’‘注意局面’等空话。"
        u8"先比较实战后惩罚线与推荐着应对线，但只能解释输入中已经给出的中文棋谱、验证变化和评分差异。"
        u8"本请求不提供可靠的棋盘空间信息：不得根据记忆、坐标棋谱或棋盘编码推测棋子位置、行棋路线、攻击、保护、"
        u8"将军、吃子、交换或弱点，也不得自行计算新变化。"
        u8"若两条引擎变化不足以证明具体战术原因，必须明确写‘现有引擎变化只确认评分恶化，请在棋盘回放中核对具体子力关系’，"
        u8"不得虚构惩罚。"
        u8"学习者当时思路是对其计算过程的线索：只在证据支持时说明哪里漏算或误判，不得做人格判断。"
        u8"必须输出且只输出一个 JSON 对象，包含四个字符串字段："
        u8"diagnosis：80字以内，只说明两条已验证变化和评分所能确认的差异；"
        u8"evidence：140字以内，引用实战后与推荐后的已验证中文棋谱；"
        u8"training_task：100字以内，要求学习者在棋盘回放中对比这两条变化，不得补充未验证的棋局事实；"
        u8"reflection_question：60字以内，写成一个可执行的落子前检查标准，不要使用问号。"
        u8"JSON 字段名必须保持 diagnosis、evidence、training_task、reflection_question，不要输出 Markdown。"
        u8"禁止输出 a0-i9、f5-c3 等坐标着法；所有走法必须使用中文棋谱，例如车九进一。");

    const QString userPrompt = QString::fromUtf8(
        u8"请根据以下结构化证据输出 JSON 教练建议：\n"
        u8"步数：%1\n实际走法：%2\n引擎推荐：%3\n"
        u8"最佳评分：%6\n实际评分：%7\n局面损失：%8\n错误等级：%9\n"
        u8"思考时间：%10 毫秒\n实战后惩罚线：%11\n推荐着应对线：%12\n"
        u8"长期统计：累计对局 %13，已分析红方走法 %14，平均损失 %15，严重失误 %16，"
        u8"主动悔棋 %17 次，其中已确认明显/严重失误 %18 次。")
        .arg(analysis.ply)
        .arg(analysis.actualNotation, analysis.bestNotation)
        .arg(analysis.bestScore)
        .arg(analysis.actualScore)
        .arg(analysis.scoreLoss)
        .arg(analysis.category)
        .arg(analysis.thinkingTimeMs)
        .arg(analysis.actualPrincipalVariation.isEmpty()
                 ? QString::fromUtf8(u8"引擎未返回足够的实战后变化")
                 : analysis.actualPrincipalVariation)
        .arg(analysis.principalVariation)
        .arg(stats.games)
        .arg(stats.analyzedMoves)
        .arg(stats.averageLoss, 0, 'f', 1)
        .arg(stats.blunders)
        .arg(stats.undoEvents)
        .arg(stats.blunderUndoEvents);
    const QString contextualPrompt = userPrompt + QString::fromUtf8(
        u8"\n\n学习者在落子前留下的思路（仅作其意图线索，不能作为棋局事实）：\n%1")
        .arg(request.gameContext.trimmed().isEmpty()
                 ? QString::fromUtf8(u8"未记录")
                 : request.gameContext.trimmed().left(600));

    QJsonObject body;
    body["model"] = QString::fromLatin1(deepSeekModel);
    body["stream"] = false;
    body["max_tokens"] = 650;
    body["temperature"] = 0.1;
    body["response_format"] = QJsonObject{{"type", "json_object"}};
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"}, {"content", contextualPrompt}}
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray DeepSeekCoach::makeGameReviewRequestBody(
    const GameReviewRequest &request)
{
    const auto &context = request.context;
    const auto &stats = request.stats;
    const QString systemPrompt = QString::fromUtf8(
        u8"你是一名直接、严谨的中国象棋复盘教练。Pikafish 已负责棋局计算；"
        u8"你只能依据完整棋谱、评分统计和关键转折点进行教学总结，不得编造变化或修改引擎结论。"
        u8"悔棋是最高优先级学习证据：只要输入中存在悔棋，就必须逐条分析被撤销的着法为什么不好、"
        u8"对手如何惩罚、推荐着法解决什么，并指出‘落子后才发现’暴露的检查步骤缺口。不得遗漏任何一条悔棋。"
        u8"禁止寒暄、鼓励、按顺序复述整盘棋、重复同一评分，以及没有证据的性格判断。"
        u8"只保留最影响胜负和最可能复发的内容。输出且只输出一个 JSON 对象，包含六个字符串字段："
        u8"overview：80字以内，直接给本局最主要结论；turning_points：最多3个非悔棋转折点，另加全部悔棋，"
        u8"每个都写实际着、惩罚和推荐着；"
        u8"strengths：最多2项且必须有步数证据；recurring_pattern：只写有至少2条证据支持的模式，否则写证据不足；"
        u8"training_plan：恰好2项任务，每项包含训练次数和成功标准；reflection_question：只问一个关键漏算点，30字以内。"
        u8"示例 JSON：{\"overview\":\"...\",\"turning_points\":\"...\","
        u8"\"strengths\":\"...\",\"recurring_pattern\":\"...\","
        u8"\"training_plan\":\"1. ...；2. ...\",\"reflection_question\":\"...\"}"
        u8"禁止输出 a0-i9、f5-c3 等坐标着法；所有走法必须使用中文棋谱，例如车九进一。");

    const QString userPrompt = QString::fromUtf8(
        u8"请根据以下证据完成整盘复盘。\n"
        u8"对局编号：%1\n结果：%2\n结束原因：%3\n"
        u8"总手数：%4，红方走法：%5，已分析红方走法：%6\n"
        u8"本局平均损失：%7，明显失误：%8，严重失误：%9，红方平均思考：%10 秒\n\n"
        u8"各阶段表现：\n%11\n\n关键转折点（最多五个）：\n%12\n\n"
        u8"本局悔棋证据（最高优先级，必须逐条写入复盘）：\n%13\n\n当前动态画像（假设而非人格结论）：\n%14\n\n"
        u8"完整着法记录：\n%15\n\n"
        u8"长期个人统计：完成对局 %16，分析走法 %17，平均损失 %18，"
        u8"轻微失误 %19，明显失误 %20，严重失误 %21；累计悔棋 %22 次，"
        u8"其中已确认的明显/严重失误 %23 次。")
        .arg(context.gameId).arg(context.result).arg(context.endReason)
        .arg(context.totalMoves).arg(context.redMoves).arg(context.analyzedMoves)
        .arg(context.averageLoss, 0, 'f', 1)
        .arg(context.mistakes).arg(context.blunders)
        .arg(context.averageThinkingTimeMs / 1000.0, 0, 'f', 1)
        .arg(context.phaseSummary, context.keyMoments, context.undoSummary,
             context.profileSummary, context.moveTranscript)
        .arg(stats.games).arg(stats.analyzedMoves)
        .arg(stats.averageLoss, 0, 'f', 1)
        .arg(stats.inaccuracies).arg(stats.mistakes).arg(stats.blunders)
        .arg(stats.undoEvents).arg(stats.blunderUndoEvents);

    QJsonObject body;
    body["model"] = QString::fromLatin1(deepSeekModel);
    body["stream"] = false;
    body["max_tokens"] = 1300;
    body["temperature"] = request.attempt == 0 ? 0.15 : 0.0;
    body["response_format"] = QJsonObject{{"type", "json_object"}};
    body["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{{"role", "user"},
                    {"content", request.attempt == 0
                                    ? userPrompt
                                    : userPrompt + QString::fromUtf8(
                                          u8"\n\n上一次响应为空或格式错误。请直接输出完整 JSON 对象，不要使用 Markdown 代码块，也不要添加解释。")}}
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
    const QJsonObject choice = choices[0].toObject();
    const QString content = choice.value("message").toObject()
                                .value("content").toString();
    if (content.trimmed().isEmpty()) {
        const QString finishReason = choice.value("finish_reason").toString();
        *errorMessage = QString::fromUtf8(u8"模型返回了空内容（finish_reason=%1）")
                            .arg(finishReason.isEmpty()
                                     ? QStringLiteral("unknown")
                                     : finishReason);
        return false;
    }

    QJsonObject coaching;
    if (!JsonObjectExtractor::parse(content, &coaching, errorMessage)) {
        return false;
    }
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
    result->model = QString::fromLatin1(deepSeekModel);
    result->diagnosis = diagnosis;
    result->evidence = evidence;
    result->trainingTask = trainingTask;
    result->reflectionQuestion = reflectionQuestion;
    return true;
}

bool DeepSeekCoach::parseGameReviewContent(
    const QByteArray &body,
    const GameReviewRequest &request,
    GameReviewResult *result,
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
    const QJsonObject choice = choices[0].toObject();
    const QString content = choice.value("message").toObject()
                                .value("content").toString();
    if (content.trimmed().isEmpty()) {
        const QString finishReason = choice.value("finish_reason").toString();
        *errorMessage = QString::fromUtf8(u8"模型返回了空内容（finish_reason=%1）")
                            .arg(finishReason.isEmpty()
                                     ? QStringLiteral("unknown")
                                     : finishReason);
        return false;
    }
    QJsonObject review;
    if (!JsonObjectExtractor::parse(content, &review, errorMessage)) {
        return false;
    }
    const QString overview = review.value("overview").toString().trimmed();
    const QString turningPoints = review.value("turning_points").toString().trimmed();
    const QString strengths = review.value("strengths").toString().trimmed();
    const QString recurringPattern = review.value("recurring_pattern").toString().trimmed();
    const QString trainingPlan = review.value("training_plan").toString().trimmed();
    const QString reflectionQuestion = review.value("reflection_question").toString().trimmed();
    if (overview.isEmpty() || turningPoints.isEmpty() || strengths.isEmpty()
        || recurringPattern.isEmpty() || trainingPlan.isEmpty()
        || reflectionQuestion.isEmpty()) {
        *errorMessage = QString::fromUtf8(u8"JSON 缺少整盘复盘必要字段");
        return false;
    }

    result->gameId = request.context.gameId;
    result->userId = request.context.userId;
    result->model = QString::fromLatin1(deepSeekModel);
    result->overview = overview;
    result->turningPoints = turningPoints;
    result->strengths = strengths;
    result->recurringPattern = recurringPattern;
    result->trainingPlan = trainingPlan;
    result->reflectionQuestion = reflectionQuestion;
    return true;
}
