#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include "game_database.h"
#include "xiangqi_game.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        "multi-user-profile-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStandardPaths::setTestModeEnabled(true);

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDirectory);
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", "legacy-profile-test");
        legacy.setDatabaseName(QDir(dataDirectory).filePath("xiangqi_training.db"));
        if (!legacy.open()) return 1;
        QSqlQuery query(legacy);
        if (!query.exec("CREATE TABLE games (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                        "started_at TEXT NOT NULL, finished_at TEXT, "
                        "result TEXT NOT NULL DEFAULT 'ongoing')") ||
            !query.exec("INSERT INTO games(started_at, finished_at, result) "
                        "VALUES('old-start','old-finish','red_wins')")) {
            return 1;
        }
        legacy.close();
    }
    QSqlDatabase::removeDatabase("legacy-profile-test");

    GameDatabase database;
    QString error;
    if (!database.open(&error) || database.users().size() != 1) {
        return 1;
    }
    if (database.userProfile(1).completedGames != 1 ||
        database.userProfile(1).wins != 1) {
        return 1;
    }
    const qint64 alice = database.createUser(QString::fromUtf8(u8"甲用户"), &error);
    const qint64 bob = database.createUser(QString::fromUtf8(u8"乙用户"), &error);
    if (alice < 0 || bob < 0 || alice == bob || database.users().size() != 3) {
        return 2;
    }
    if (!database.setSelectedUserId(alice, &error) || database.selectedUserId() != alice) {
        return 3;
    }

    for (int index = 0; index < 10; ++index) {
        const qint64 gameId = database.startGame(alice, &error);
        if (gameId < 0 ||
            !database.finishGame(gameId, index < 6 ? XiangqiGame::GameResult::RedWins
                                                   : XiangqiGame::GameResult::BlackWins,
                                 &error)) {
            return 4;
        }
    }
    for (int index = 0; index < 3; ++index) {
        const qint64 gameId = database.startGame(bob, &error);
        if (gameId < 0 ||
            !database.finishGame(gameId, XiangqiGame::GameResult::BlackWins, &error)) {
            return 5;
        }
    }

    const auto aliceProfile = database.userProfile(alice);
    const auto bobProfile = database.userProfile(bob);
    if (aliceProfile.completedGames != 10 || aliceProfile.wins != 6 ||
        aliceProfile.losses != 4 || bobProfile.completedGames != 3 ||
        bobProfile.losses != 3) {
        return 6;
    }
    if (database.recentGamePerformance(alice, 10).size() != 10 ||
        database.recentGamePerformance(bob, 10).size() != 3) {
        return 6;
    }

    bool created = false;
    const auto report = database.generateMilestoneReport(alice, &created, &error);
    if (!created || report.throughGames != 10 || report.summary.isEmpty()) {
        return 7;
    }
    created = true;
    const auto duplicate = database.generateMilestoneReport(alice, &created, &error);
    if (created || duplicate.id != report.id ||
        database.generateMilestoneReport(bob).id >= 0) {
        return 8;
    }

    XiangqiGame game;
    if (!game.move(6, 0, 5, 0, 1500)) {
        return 9;
    }
    const auto move = game.moveHistory().front();
    const qint64 trainingGame = database.startGame(alice, &error);
    if (trainingGame < 0 || !database.recordMove(trainingGame, move, &error) ||
        !database.recordAnalysis(trainingGame, 1, "a3a4", "b2b3",
                                 120, 0, 120, "mistake", "b2b3", &error) ||
        database.generateTrainingPositions(alice, &error) < 0) {
        return 10;
    }
    const auto aliceTraining = database.dueTrainingPositions(alice, 5);
    const auto bobTraining = database.dueTrainingPositions(bob, 5);
    if (aliceTraining.isEmpty() || !bobTraining.isEmpty()) {
        return 11;
    }
    if (!database.recordTrainingAttempt(aliceTraining.front().id,
                                        aliceTraining.front().bestMove,
                                        true, 2100, &error) ||
        database.trainingSummary(alice).attempts != 1 ||
        database.trainingSummary(bob).attempts != 0) {
        return 12;
    }
    if (!database.finishGame(trainingGame, XiangqiGame::GameResult::RedWins, &error)) {
        return 13;
    }
    GameDatabase::GameReviewContext context;
    if (!database.buildGameReviewContext(trainingGame, &context, &error)
        || context.userId != alice || context.totalMoves != 1
        || context.redMoves != 1 || context.analyzedMoves != 1
        || !context.moveTranscript.contains("a3a4")
        || context.keyMoments.isEmpty()) {
        return 14;
    }
    GameDatabase::GameReview review;
    review.gameId = trainingGame;
    review.model = "mock-reviewer";
    review.overview = QString::fromUtf8(u8"整体评价");
    review.turningPoints = QString::fromUtf8(u8"关键转折");
    review.strengths = QString::fromUtf8(u8"优点");
    review.recurringPattern = QString::fromUtf8(u8"思考模式");
    review.trainingPlan = QString::fromUtf8(u8"训练计划");
    review.reflectionQuestion = QString::fromUtf8(u8"复盘问题");
    if (!database.recordGameReview(review, &error)
        || !database.hasGameReview(trainingGame)
        || database.gameReview(trainingGame).trainingPlan != review.trainingPlan) {
        return 15;
    }
    if (!database.truncateGame(trainingGame, 0, &error)
        || database.hasGameReview(trainingGame)
        || database.buildGameReviewContext(trainingGame, &context)) {
        return 16;
    }
    return 0;
}
