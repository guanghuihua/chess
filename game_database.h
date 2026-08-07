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
        int undoEvents = 0;
        int analyzedUndoEvents = 0;
        int blunderUndoEvents = 0;
        double averageLoss = 0.0;
    };

    struct UndoEvent
    {
        qint64 id = -1;
        qint64 gameId = -1;
        qint64 userId = -1;
        int lastKeptPly = 0;
        int undonePlies = 0;
        int redMovePly = 0;
        QString actualMove;
        QString boardBefore;
        QString bestMove;
        int scoreLoss = 0;
        QString category;
        QString principalVariation;
        bool hadAnalysis = false;
        QString diagnosis;
        QString evidence;
        QString trainingTask;
        QString reflectionQuestion;
        QString requestedAt;
    };

    struct TrainingPosition
    {
        qint64 id = -1;
        qint64 sourceGameId = -1;
        qint64 sourcePly = 0;
        QString board;
        QString bestMove;
        QString actualMove;
        int scoreLoss = 0;
        QString category;
        QString principalVariation;
        QString theme;
        QString diagnosisTag;
        QString recommendationReason;
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
        int undoEvents = 0;
        int blunderUndoEvents = 0;
        QString mainWeakness;
    };

    struct ProfileDimension
    {
        QString dimension;
        QString title;
        int score = 0;
        double confidence = 0.0;
        QString status;
        QString trend;
        int evidenceCount = 0;
        int gameCount = 0;
        QString hypothesis;
        QString recommendedTraining;
    };

    struct TrainingPlanItem
    {
        qint64 id = -1;
        QString diagnosisTag;
        QString title;
        int targetCount = 0;
        int completedCount = 0;
        QString reason;
    };

    struct TrainingPlan
    {
        qint64 id = -1;
        qint64 userId = -1;
        int throughGames = 0;
        QString focusDimension;
        QString hypothesis;
        double confidence = 0.0;
        QString status;
        QString successMetric;
        int reviewAfterGames = 5;
        QString createdAt;
        QVector<TrainingPlanItem> items;
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

    struct GameSummary
    {
        qint64 id = -1;
        int sequenceNumber = 0;
        QString startedAt;
        QString finishedAt;
        QString result;
        QString endReason;
        int moveCount = 0;
        bool hasReview = false;
    };

    struct RecordedMove
    {
        int ply = 0;
        QString side;
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;
        QString movedPiece;
        QString capturedPiece;
        qint64 thinkingTimeMs = 0;
        QString boardBefore;
        QString boardAfter;
        QString actualMove;
        QString bestMove;
        int bestScore = 0;
        int actualScore = 0;
        int scoreLoss = 0;
        QString category;
        QString principalVariation;
        QString diagnosis;
        QString evidence;
        QString trainingTask;
        QString reflectionQuestion;
        bool hasAnalysis = false;
    };

    struct GameReviewContext
    {
        qint64 gameId = -1;
        qint64 userId = -1;
        QString result;
        QString endReason;
        int totalMoves = 0;
        int redMoves = 0;
        int analyzedMoves = 0;
        double averageLoss = 0.0;
        double averageThinkingTimeMs = 0.0;
        int mistakes = 0;
        int blunders = 0;
        QString moveTranscript;
        QString phaseSummary;
        QString keyMoments;
        QString undoSummary;
        QString profileSummary;
    };

    struct GameReview
    {
        qint64 gameId = -1;
        QString model;
        QString overview;
        QString turningPoints;
        QString strengths;
        QString recurringPattern;
        QString trainingPlan;
        QString reflectionQuestion;
        QString createdAt;
    };

    struct ChatMessage
    {
        qint64 id = -1;
        qint64 gameId = -1;
        int ply = 0;
        QString role;
        QString content;
        QString createdAt;
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
    bool finishGame(qint64 gameId, XiangqiGame::GameResult result,
                    const QString &endReason,
                    QString *errorMessage = nullptr);
    bool truncateGame(qint64 gameId, int lastKeptPly,
                      QString *errorMessage = nullptr);
    bool recordUndoEvent(qint64 gameId, int lastKeptPly, int undonePlies,
                         QString *errorMessage = nullptr);
    QVector<UndoEvent> undoEvents(qint64 userId, int limit = 50) const;
    QVector<UndoEvent> gameUndoEvents(qint64 gameId) const;
    bool attachAnalysisToUndoEvent(qint64 gameId, int ply,
                                   const QString &actualMove,
                                   const QString &bestMove, int scoreLoss,
                                   const QString &category,
                                   const QString &principalVariation,
                                   bool *matched = nullptr,
                                   QString *errorMessage = nullptr);
    bool attachCoachingToUndoEvent(qint64 gameId, int ply,
                                   const QString &actualMove,
                                   const QString &diagnosis,
                                   const QString &evidence,
                                   const QString &trainingTask,
                                   const QString &reflectionQuestion,
                                   bool *matched = nullptr,
                                   QString *errorMessage = nullptr);
    bool recordAnalysis(qint64 gameId, int ply, const QString &actualMove,
                        const QString &bestMove, int bestScore, int actualScore,
                        int scoreLoss, const QString &category,
                        const QString &principalVariation,
                        QString *errorMessage = nullptr);
    bool recordAnalysis(qint64 gameId, int ply, const QString &actualMove,
                        const QString &bestMove, int bestScore, int actualScore,
                        int scoreLoss, const QString &category,
                        const QString &principalVariation,
                        const QString &rawPrincipalVariation,
                        QString *errorMessage = nullptr);
    bool recordCoaching(qint64 gameId, int ply, const QString &model,
                        const QString &diagnosis, const QString &evidence,
                        const QString &trainingTask,
                        const QString &reflectionQuestion,
                        QString *errorMessage = nullptr);
    QString moveCoachingContext(qint64 gameId, int throughPly) const;
    bool buildGameReviewContext(qint64 gameId, GameReviewContext *context,
                                QString *errorMessage = nullptr) const;
    bool recordGameReview(const GameReview &review,
                          QString *errorMessage = nullptr);
    bool hasGameReview(qint64 gameId) const;
    GameReview gameReview(qint64 gameId) const;
    int generateTrainingPositions(qint64 userId, QString *errorMessage = nullptr);
    QString trainingGenerationContext(qint64 userId) const;
    qint64 storeGeneratedTrainingPosition(qint64 userId, const QString &board,
                                          const QString &bestMove,
                                          const QString &principalVariation,
                                          int engineScore, const QString &theme,
                                          const QString &diagnosisTag,
                                          const QString &learningGoal,
                                          const QString &hint,
                                          QString *errorMessage = nullptr);
    QVector<TrainingPosition> dueTrainingPositions(qint64 userId, int limit = 10) const;
    bool recordTrainingAttempt(qint64 positionId, const QString &attemptedMove,
                               bool correct, qint64 thinkingTimeMs,
                               QString *errorMessage = nullptr);
    bool recordTrainingAttempt(qint64 positionId, const QString &attemptedMove,
                               bool correct, qint64 thinkingTimeMs, int hintCount,
                               QString *errorMessage = nullptr);
    TrainingSummary trainingSummary(qint64 userId) const;

    TrainingStats trainingStats(qint64 userId) const;
    UserProfile userProfile(qint64 userId) const;
    ProfileReport generateMilestoneReport(qint64 userId, bool *created = nullptr,
                                          QString *errorMessage = nullptr);
    QVector<ProfileReport> profileReports(qint64 userId) const;
    QVector<GamePerformance> recentGamePerformance(qint64 userId,
                                                   int limit = 10) const;
    QVector<GameSummary> completedGames(qint64 userId, int limit = 100) const;
    QVector<RecordedMove> recordedMoves(qint64 gameId) const;
    bool recordChatMessage(qint64 userId, qint64 gameId, int ply,
                           const QString &role, const QString &content,
                           QString *errorMessage = nullptr);
    QVector<ChatMessage> chatMessages(qint64 userId, qint64 gameId,
                                      int ply = -1) const;
    bool deleteCompletedGame(qint64 userId, qint64 gameId,
                             QString *errorMessage = nullptr);
    bool rebuildPersonalization(qint64 userId, QString *errorMessage = nullptr);
    QVector<ProfileDimension> profileDimensions(qint64 userId) const;
    TrainingPlan currentTrainingPlan(qint64 userId) const;
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
