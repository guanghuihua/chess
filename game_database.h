#ifndef GAME_DATABASE_H
#define GAME_DATABASE_H

#include <QtGlobal>
#include <QSqlDatabase>
#include <QString>

#include "xiangqi_game.h"

class GameDatabase
{
public:
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

    GameDatabase();
    ~GameDatabase();

    bool open(QString *errorMessage = nullptr);
    qint64 startGame(QString *errorMessage = nullptr);
    bool recordMove(qint64 gameId, const XiangqiGame::MoveRecord &move,
                    QString *errorMessage = nullptr);
    bool finishGame(qint64 gameId, XiangqiGame::GameResult result,
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

    TrainingStats trainingStats() const;
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
