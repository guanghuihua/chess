#ifndef DEEPSEEK_COACH_H
#define DEEPSEEK_COACH_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QString>

#include "game_database.h"
#include "pikafish_analyzer.h"

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

    explicit DeepSeekCoach(QObject *parent = nullptr);

    bool isConfigured() const;
    bool saveApiKey(const QString &apiKey, QString *errorMessage = nullptr);
    bool removeApiKey(QString *errorMessage = nullptr);
    void testConnection();
    void requestCoaching(const PikafishAnalyzer::AnalysisResult &analysis,
                         const GameDatabase::TrainingStats &stats);
    void requestGameReview(const GameDatabase::GameReviewContext &context,
                           const GameDatabase::TrainingStats &stats);

signals:
    void coachingReady(const DeepSeekCoach::CoachingResult &result);
    void gameReviewReady(const DeepSeekCoach::GameReviewResult &result);
    void statusChanged(const QString &message, bool available);
    void connectionTested(bool success, const QString &message);

private:
    struct Request
    {
        PikafishAnalyzer::AnalysisResult analysis;
        GameDatabase::TrainingStats stats;
    };

    struct GameReviewRequest
    {
        GameDatabase::GameReviewContext context;
        GameDatabase::TrainingStats stats;
    };

    QNetworkAccessManager network_;
    QQueue<Request> requests_;
    QQueue<GameReviewRequest> game_review_requests_;
    QString api_key_;
    bool busy_ = false;

    void processNext();
    void processNextGameReview();
    void handleReply(class QNetworkReply *reply, const Request &request);
    void handleGameReviewReply(class QNetworkReply *reply,
                               const GameReviewRequest &request);
    static QByteArray makeRequestBody(const Request &request);
    static QByteArray makeGameReviewRequestBody(const GameReviewRequest &request);
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

#endif // DEEPSEEK_COACH_H
