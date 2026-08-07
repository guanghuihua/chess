#ifndef DEEPSEEK_COACH_H
#define DEEPSEEK_COACH_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QString>

#include "game_database.h"
#include "pikafish_analyzer.h"

class QUrl;

class DeepSeekCoach : public QObject
{
    Q_OBJECT

public:
    struct CoachingResult
    {
        qint64 gameId = -1;
        int ply = 0;
        QString actualMove;
        QString model;
        QString diagnosis;
        QString evidence;
        QString trainingTask;
        QString reflectionQuestion;
    };

    struct GameReviewResult
    {
        qint64 gameId = -1;
        qint64 userId = -1;
        QString model;
        QString overview;
        QString turningPoints;
        QString strengths;
        QString recurringPattern;
        QString trainingPlan;
        QString reflectionQuestion;
    };

    struct ExerciseDraft
    {
        QString requestId;
        QString board;
        QString theme;
        QString diagnosisTag;
        QString learningGoal;
        QString hint;
    };

    explicit DeepSeekCoach(QObject *parent = nullptr);

    bool isConfigured() const;
    bool saveApiKey(const QString &apiKey, QString *errorMessage = nullptr);
    bool removeApiKey(QString *errorMessage = nullptr);
    void testConnection();
    void requestCoaching(const PikafishAnalyzer::AnalysisResult &analysis,
                         const GameDatabase::TrainingStats &stats,
                         const QString &gameContext = QString());
    void requestGameReview(const GameDatabase::GameReviewContext &context,
                           const GameDatabase::TrainingStats &stats);
    void requestChat(const QString &requestId, const QString &evidenceContext,
                     const QString &conversationHistory,
                     const QString &question);
    void requestGeneratedExercise(const QString &requestId,
                                  const QString &profileContext);

signals:
    void coachingReady(const DeepSeekCoach::CoachingResult &result);
    void gameReviewReady(const DeepSeekCoach::GameReviewResult &result);
    void chatReplyReady(const QString &requestId, const QString &answer,
                        const QString &errorMessage);
    void exerciseDraftReady(const DeepSeekCoach::ExerciseDraft &draft,
                            const QString &errorMessage);
    void statusChanged(const QString &message, bool available);
    void connectionTested(bool success, const QString &message);

private:
    struct Request
    {
        PikafishAnalyzer::AnalysisResult analysis;
        GameDatabase::TrainingStats stats;
        QString gameContext;
    };

    struct GameReviewRequest
    {
        GameDatabase::GameReviewContext context;
        GameDatabase::TrainingStats stats;
        int attempt = 0;
    };

    struct ChatRequest
    {
        QString requestId;
        QString evidenceContext;
        QString conversationHistory;
        QString question;
    };

    struct ExerciseRequest
    {
        QString requestId;
        QString profileContext;
    };

    QNetworkAccessManager network_;
    QQueue<Request> requests_;
    QQueue<GameReviewRequest> game_review_requests_;
    QQueue<ChatRequest> chat_requests_;
    QQueue<ExerciseRequest> exercise_requests_;
    QString api_key_;
    bool packy_mode_ = false;
    bool busy_ = false;

    void processNext();
    void processNextGameReview();
    void processNextChat();
    void processNextExercise();
    void handleReply(class QNetworkReply *reply, const Request &request);
    void handleGameReviewReply(class QNetworkReply *reply,
                               const GameReviewRequest &request);
    void retryOrFallbackGameReview(const GameReviewRequest &request,
                                   const QString &reason);
    void handleChatReply(class QNetworkReply *reply, const ChatRequest &request);
    void handleExerciseReply(class QNetworkReply *reply, const ExerciseRequest &request);
    static QByteArray makeChatRequestBody(const ChatRequest &request);
    static QByteArray makeExerciseRequestBody(const ExerciseRequest &request);
    static QByteArray makeRequestBody(const Request &request);
    static QByteArray makeGameReviewRequestBody(const GameReviewRequest &request);
    QByteArray providerRequestBody(const QByteArray &chatCompletionsBody,
                                   bool wholeGame) const;
    QByteArray normalizedResponseBody(const QByteArray &body) const;
    QString activeFastModel() const;
    QString activeReviewModel() const;
    QUrl activeEndpoint() const;
    static bool parseCoachingContent(const QByteArray &body,
                                     const Request &request,
                                     CoachingResult *result,
                                     QString *errorMessage);
    static bool parseGameReviewContent(const QByteArray &body,
                                       const GameReviewRequest &request,
                                       GameReviewResult *result,
                                       QString *errorMessage);
};

Q_DECLARE_METATYPE(DeepSeekCoach::CoachingResult)
Q_DECLARE_METATYPE(DeepSeekCoach::GameReviewResult)
Q_DECLARE_METATYPE(DeepSeekCoach::ExerciseDraft)

#endif // DEEPSEEK_COACH_H
