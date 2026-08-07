#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>
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
    const qint64 gameId = database.startGame(1, &error);
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
        if (result.principalVariation.contains(
                QRegularExpression("[a-i][0-9][a-i][0-9]"))) {
            qCritical() << "Principal variation was not translated"
                        << result.principalVariation;
            application.exit(6);
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
        const QString trainingBest = result.bestMove == result.actualMove
                                         ? QStringLiteral("b2b3")
                                         : result.bestMove;
        if (!database.recordAnalysis(
                result.gameId, result.ply, result.actualMove, trainingBest,
                120, 0, 120, "mistake", trainingBest,
                "a3a4 a6a5 b3b4", &error)) {
            qCritical() << "Training analysis setup failed:" << error;
            application.exit(8);
            return;
        }
        if (database.generateTrainingPositions(1, &error) < 0) {
            qCritical() << "Training position generation failed:" << error;
            application.exit(9);
            return;
        }
        const qint64 generatedId = database.storeGeneratedTrainingPosition(
            1, QString::fromStdString(move.boardBefore), trainingBest,
            result.principalVariation, 42, QString::fromUtf8(u8"威胁识别"),
            QStringLiteral("missed_threat"), QString::fromUtf8(u8"训练先检查对方直接威胁"),
            QString::fromUtf8(u8"先列出对方将军、吃子和强制威胁"), &error);
        if (generatedId < 0) {
            qCritical() << "AI generated training position setup failed:" << error;
            application.exit(10);
            return;
        }
        const auto positions = database.dueTrainingPositions(1, 1000);
        auto profileVariation = positions.cend();
        auto aiGenerated = positions.cend();
        for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
            if (it->theme.contains(QString::fromUtf8(u8"画像变式"))) {
                profileVariation = it;
            }
            if (it->id == generatedId && it->sourcePly < 0) {
                aiGenerated = it;
            }
        }
        if (positions.isEmpty() || profileVariation == positions.cend()
            || aiGenerated == positions.cend()) {
            qCritical() << "Expected generated training positions were missing"
                        << positions.size()
                        << (profileVariation != positions.cend())
                        << (aiGenerated != positions.cend());
            application.exit(10);
            return;
        }
        if (!database.recordTrainingAttempt(profileVariation->id,
                                            profileVariation->bestMove,
                                            true, 2500, &error)) {
            qCritical() << "Training attempt failed:" << profileVariation->id
                        << profileVariation->sourcePly << error;
            application.exit(10);
            return;
        }
        const auto trainingSummary = database.trainingSummary(1);
        if (trainingSummary.positions < 1 || trainingSummary.attempts < 1 ||
            trainingSummary.correctAttempts < 1) {
            qCritical() << "Training summary was not updated";
            application.exit(11);
            return;
        }
        const auto stats = database.trainingStats(1);
        if (stats.analyzedMoves < 1 || stats.coachedMoves < 1) {
            qCritical() << "Training statistics were not updated";
            application.exit(8);
            return;
        }
        qInfo() << "PASS" << result.actualMove << result.bestMove
                << result.scoreLoss << result.category
                << result.principalVariation;
        application.exit(0);
    });

    QTimer::singleShot(0, [&] { analyzer.analyzeMove(gameId, move); });
    QTimer::singleShot(15000, [&] {
        qCritical() << "Timed out waiting for Pikafish";
        application.exit(9);
    });
    return application.exec();
}
