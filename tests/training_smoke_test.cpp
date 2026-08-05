#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QTimer>

#include "game_database.h"
#include "pikafish_analyzer.h"
#include "xiangqi_game.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName("xiangqi-training-smoke-test");
    QStandardPaths::setTestModeEnabled(true);

    XiangqiGame game;
    if (!game.move(6, 0, 5, 0, 1234) || game.moveHistory().size() != 1) {
        qCritical() << "Move recording failed";
        return 1;
    }

    const XiangqiGame::MoveRecord &move = game.moveHistory().back();
    if (move.thinkingTimeMs != 1234 || move.boardBefore.empty() || move.boardAfter.empty()) {
        qCritical() << "Move record is incomplete";
        return 2;
    }

    GameDatabase database;
    QString error;
    if (!database.open(&error)) {
        qCritical() << "Database open failed:" << error;
        return 3;
    }
    const qint64 gameId = database.startGame(&error);
    if (gameId < 0 || !database.recordMove(gameId, move, &error)) {
        qCritical() << "Database move write failed:" << error;
        return 4;
    }

    PikafishAnalyzer analyzer;
    QObject::connect(&analyzer, &PikafishAnalyzer::statusChanged,
                     [](const QString &status, bool) { qInfo() << status; });
    QObject::connect(&analyzer, &PikafishAnalyzer::analysisReady,
                     [&](const PikafishAnalyzer::AnalysisResult &result) {
        if (result.bestMove.isEmpty() || result.actualMove != "a3a4") {
            qCritical() << "Unexpected analysis result" << result.actualMove << result.bestMove;
            application.exit(5);
            return;
        }
        if (!database.recordAnalysis(
                result.gameId, result.ply, result.actualMove, result.bestMove,
                result.bestScore, result.actualScore, result.scoreLoss,
                result.category, result.principalVariation, &error)) {
            qCritical() << "Database analysis write failed:" << error;
            application.exit(6);
            return;
        }
        if (!database.recordCoaching(
                result.gameId, result.ply, "mock-coach",
                QString::fromUtf8(u8"测试诊断"), QString::fromUtf8(u8"测试依据"),
                QString::fromUtf8(u8"测试任务"), QString::fromUtf8(u8"测试问题"),
                &error)) {
            qCritical() << "Database coaching write failed:" << error;
            application.exit(7);
            return;
        }
        const auto stats = database.trainingStats();
        if (stats.analyzedMoves < 1 || stats.coachedMoves < 1) {
            qCritical() << "Training statistics were not updated";
            application.exit(8);
            return;
        }
        qInfo() << "PASS" << result.actualMove << result.bestMove
                << result.scoreLoss << result.category;
        application.exit(0);
    });

    QTimer::singleShot(0, [&] { analyzer.analyzeMove(gameId, move); });
    QTimer::singleShot(15000, [&] {
        qCritical() << "Timed out waiting for Pikafish";
        application.exit(9);
    });
    return application.exec();
}
