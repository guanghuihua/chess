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

    explicit DeepSeekCoach(QObject *parent = nullptr);

    bool isConfigured() const;
    bool saveApiKey(const QString &apiKey, QString *errorMessage = nullptr);
    bool removeApiKey(QString *errorMessage = nullptr);
    void testConnection();
    void requestCoaching(const PikafishAnalyzer::AnalysisResult &analysis,
                         const GameDatabase::TrainingStats &stats);

signals:
    void coachingReady(const DeepSeekCoach::CoachingResult &result);
    void statusChanged(const QString &message, bool available);
    void connectionTested(bool success, const QString &message);

private:
    struct Request
    {
        PikafishAnalyzer::AnalysisResult analysis;
        GameDatabase::TrainingStats stats;
    };

    QNetworkAccessManager network_;
    QQueue<Request> requests_;
    QString api_key_;
    bool busy_ = false;

    void processNext();
    void handleReply(class QNetworkReply *reply, const Request &request);
    static QByteArray makeRequestBody(const Request &request);
    static bool parseCoachingContent(const QByteArray &body,
                                     const Request &request,
                                     CoachingResult *result,
                                     QString *errorMessage);
};

Q_DECLARE_METATYPE(DeepSeekCoach::CoachingResult)

#endif // DEEPSEEK_COACH_H
