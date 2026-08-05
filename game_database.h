#ifndef GAME_DATABASE_H
#define GAME_DATABASE_H

#include <QtGlobal>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "xiangqi_game.h"

class GameDatabase
{
public:
    struct User
    {
        qint64 id = -1;
        QString name;
        QString createdAt;
    };

    struct TrainingStats
    {
        int games = 0;
        int analyzedMoves = 0;
        int coachedMoves = 0;
        int excellentMoves = 0;
        int inaccuracies = 0;
        int mistakes = 0;
        int blunders = 0;
        double averageLoss = 0.0;
    };

    struct TrainingPosition
    {
        qint64 id = -1;
        qint64 sourceGameId = -1;
        int sourcePly = 0;
        QString board;
        QString bestMove;
        QString actualMove;
        int scoreLoss = 0;
        QString category;
        QString principalVariation;
        QString theme;
        int mastery = 0;
        int attempts = 0;
        int correctAttempts = 0;
    };

    struct TrainingSummary
    {
        int positions = 0;
        int due = 0;
        int attempts = 0;
        int correctAttempts = 0;
    };

    struct UserProfile
    {
        int completedGames = 0;
        int wins = 0;
        int losses = 0;
        int draws = 0;
        int analyzedMoves = 0;
        double averageLoss = 0.0;
        double averageThinkingTimeMs = 0.0;
        int blunders = 0;
        int trainingAttempts = 0;
        int trainingCorrect = 0;
        QString mainWeakness;
    };

    struct ProfileReport
    {
        qint64 id = -1;
        qint64 userId = -1;
        int throughGames = 0;
        QString summary;
        QString generatedAt;
    };

    struct GamePerformance
    {
        qint64 gameId = -1;
        QString result;
        double averageLoss = 0.0;
        int blunders = 0;
    };

    GameDatabase();
    ~GameDatabase();

    bool open(QString *errorMessage = nullptr);
    QVector<User> users() const;
    qint64 createUser(const QString &name, QString *errorMessage = nullptr);
    qint64 selectedUserId() const;
    bool setSelectedUserId(qint64 userId, QString *errorMessage = nullptr);
    qint64 startGame(qint64 userId, QString *errorMessage = nullptr);
    bool abandonGame(qint64 gameId, QString *errorMessage = nullptr);
    bool recordMove(qint64 gameId, const XiangqiGame::MoveRecord &move,
                    QString *errorMessage = nullptr);
    bool finishGame(qint64 gameId, XiangqiGame::GameResult result,
                    QString *errorMessage = nullptr);
    bool truncateGame(qint64 gameId, int lastKeptPly,
                      QString *errorMessage = nullptr);
    bool recordAnalysis(qint64 gameId, int ply, const QString &actualMove,
                        const QString &bestMove, int bestScore, int actualScore,
                        int scoreLoss, const QString &category,
                        const QString &principalVariation,
                        QString *errorMessage = nullptr);
    bool recordCoaching(qint64 gameId, int ply, const QString &model,
                        const QString &diagnosis, const QString &evidence,
                        const QString &trainingTask,
                        const QString &reflectionQuestion,
                        QString *errorMessage = nullptr);
    int generateTrainingPositions(qint64 userId, QString *errorMessage = nullptr);
    QVector<TrainingPosition> dueTrainingPositions(qint64 userId, int limit = 10) const;
    bool recordTrainingAttempt(qint64 positionId, const QString &attemptedMove,
                               bool correct, qint64 thinkingTimeMs,
                               QString *errorMessage = nullptr);
    TrainingSummary trainingSummary(qint64 userId) const;

    TrainingStats trainingStats(qint64 userId) const;
    UserProfile userProfile(qint64 userId) const;
    ProfileReport generateMilestoneReport(qint64 userId, bool *created = nullptr,
                                          QString *errorMessage = nullptr);
    QVector<ProfileReport> profileReports(qint64 userId) const;
    QVector<GamePerformance> recentGamePerformance(qint64 userId,
                                                   int limit = 10) const;
    QString databasePath() const;

private:
    QSqlDatabase database_;
    QString connection_name_;
    QString database_path_;

    bool executeSchema(QString *errorMessage);
    static QString sideName(XiangqiGame::Side side);
    static QString resultName(XiangqiGame::GameResult result);
    static QString pieceCode(const XiangqiGame::Piece &piece);
    static QString pieceCode(const std::optional<XiangqiGame::Piece> &piece);
};

#endif // GAME_DATABASE_H
