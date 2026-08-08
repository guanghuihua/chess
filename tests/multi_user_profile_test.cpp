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
        !database.recordUndoEvent(trainingGame, 0, 1, &error) ||
        database.generateTrainingPositions(alice, &error) < 0) {
        return 10;
    }
    const auto undoEvidence = database.undoEvents(alice, 5);
    if (undoEvidence.size() != 1 || undoEvidence.front().gameId != trainingGame
        || undoEvidence.front().actualMove != "a3a4"
        || undoEvidence.front().bestMove != "b2b3"
        || undoEvidence.front().scoreLoss != 120
        || !undoEvidence.front().hadAnalysis
        || !database.undoEvents(bob, 5).isEmpty()) {
        return 10;
    }
    bool matchedUndo = false;
    if (!database.attachAnalysisToUndoEvent(
            trainingGame, 1, "a3a4", "b2b3", 135, "mistake",
            "b2b3 b7b6", &matchedUndo, &error)
        || !matchedUndo
        || !database.attachCoachingToUndoEvent(
            trainingGame, 1, "a3a4", QString::fromUtf8(u8"候选着检查不足"),
            QString::fromUtf8(u8"引擎损失 135"), QString::fromUtf8(u8"重做局面"),
            QString::fromUtf8(u8"对手有什么直接反击？"), &matchedUndo, &error)
        || !matchedUndo) {
        return 10;
    }
    const auto gameUndos = database.gameUndoEvents(trainingGame);
    if (gameUndos.size() != 1 || gameUndos.front().scoreLoss != 135
        || gameUndos.front().diagnosis.isEmpty()) {
        return 10;
    }
    const QString coachingContext = database.moveCoachingContext(trainingGame, 1);
    if (!coachingContext.contains(QString::fromUtf8(u8"正式棋谱"))
        || !coachingContext.contains(QString::fromUtf8(u8"悔棋分支"))
        || !coachingContext.contains("a3a4")
        || !coachingContext.contains("b2b3")) {
        return 10;
    }
    const auto aliceTraining = database.dueTrainingPositions(alice, 5);
    const auto bobTraining = database.dueTrainingPositions(bob, 5);
    const auto dimensions = database.profileDimensions(alice);
    const auto planBeforeTraining = database.currentTrainingPlan(alice);
    if (aliceTraining.isEmpty() || !bobTraining.isEmpty()
        || aliceTraining.front().diagnosisTag == "unknown"
        || aliceTraining.front().recommendationReason.isEmpty()
        || dimensions.isEmpty() || dimensions.front().evidenceCount < 1
        || planBeforeTraining.id < 0 || planBeforeTraining.items.size() != 3
        || database.currentTrainingPlan(bob).id >= 0) {
        return 11;
    }
    if (!database.recordTrainingAttempt(aliceTraining.front().id,
                                        aliceTraining.front().bestMove,
                                        true, 2100, 2, &error) ||
        database.trainingSummary(alice).attempts != 1 ||
        database.trainingSummary(bob).attempts != 0) {
        return 12;
    }
    const auto planAfterTraining = database.currentTrainingPlan(alice);
    if (planAfterTraining.items.isEmpty()
        || planAfterTraining.items.front().completedCount < 1) {
        return 12;
    }
    if (!database.finishGame(trainingGame, XiangqiGame::GameResult::BlackWins,
                             "resignation", &error)) {
        return 13;
    }
    if (!database.recordChatMessage(alice, trainingGame, 1, "user",
                                    QString::fromUtf8(u8"为什么这一步不好？"), &error)
        || !database.recordChatMessage(alice, trainingGame, 1, "assistant",
                                       QString::fromUtf8(u8"因为忽略了直接威胁。"), &error)
        || database.chatMessages(alice, trainingGame, 1).size() != 2
        || !database.chatMessages(bob, trainingGame).isEmpty()) {
        return 13;
    }
    const auto completed = database.completedGames(alice, 20);
    const auto recorded = database.recordedMoves(trainingGame);
    const auto bobCompleted = database.completedGames(bob, 20);
    if (completed.isEmpty() || completed.front().id != trainingGame
        || recorded.size() != 1 || recorded.front().actualMove != "a3a4"
        || !recorded.front().hasAnalysis || recorded.front().bestMove != "b2b3"
        || (!bobCompleted.isEmpty() && bobCompleted.front().id == trainingGame)) {
        return 13;
    }
    for (int index = 0; index < completed.size(); ++index) {
        if (completed[index].sequenceNumber != completed.size() - index) return 13;
    }
    GameDatabase::GameReviewContext context;
    if (!database.buildGameReviewContext(trainingGame, &context, &error)
        || context.userId != alice || context.endReason != "resignation"
        || context.totalMoves != 1
        || context.redMoves != 1 || context.analyzedMoves != 1
        || !context.moveTranscript.contains("a3a4")
        || context.keyMoments.isEmpty() || !context.undoSummary.contains("a3a4")) {
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
        || database.buildGameReviewContext(trainingGame, &context)
        || database.undoEvents(alice, 5).size() != 1
        || database.gameUndoEvents(trainingGame).size() != 1
        || database.gameUndoEvents(trainingGame).front().diagnosis
               != QString::fromUtf8(u8"候选着检查不足")
        || database.gameUndoEvents(trainingGame).front().evidence
               != QString::fromUtf8(u8"引擎损失 135")
        || database.trainingStats(alice).undoEvents != 1
        || database.trainingStats(alice).blunderUndoEvents != 1
        || database.trainingStats(alice).mistakes != 1) {
        return 16;
    }
    const qint64 disposableGame = database.startGame(bob, &error);
    if (disposableGame < 0 || !database.recordMove(disposableGame, move, &error)
        || !database.recordAnalysis(disposableGame, 1, "a3a4", "b2b3",
                                    220, 0, 220, "blunder", "b2b3", &error)
        || !database.finishGame(disposableGame, XiangqiGame::GameResult::RedWins, &error)) {
        return 17;
    }
    review.gameId = disposableGame;
    if (!database.recordGameReview(review, &error)
        || !database.recordChatMessage(bob, disposableGame, 0, "user", "test", &error)
        || database.generateTrainingPositions(bob, &error) < 0
        || database.deleteCompletedGame(alice, disposableGame, &error)) {
        return 18;
    }
    error.clear();
    if (!database.deleteCompletedGame(bob, disposableGame, &error)
        || database.hasGameReview(disposableGame)
        || !database.chatMessages(bob, disposableGame).isEmpty()
        || !database.recordedMoves(disposableGame).isEmpty()
        || database.completedGames(bob, 20).size() != 3
        || database.userProfile(bob).losses != 3
        || !database.dueTrainingPositions(bob, 20).isEmpty()) {
        return 19;
    }
    GameDatabase::FavoriteScore favorite;
    favorite.userId = alice;
    favorite.title = QStringLiteral("测试收藏");
    favorite.sourceFile = QStringLiteral("sample.txt");
    favorite.sourceFormat = QStringLiteral("UCI");
    favorite.initialBoard = QStringLiteral("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR");
    favorite.sideToMove = QStringLiteral("w");
    favorite.moves = QStringLiteral("a3a4");
    favorite.rawContent = QStringLiteral("original score");
    const qint64 favoriteId = database.saveFavoriteScore(favorite, &error);
    const auto aliceFavorites = database.favoriteScores(alice);
    if (favoriteId < 0 || aliceFavorites.size() != 1
        || aliceFavorites.front().id != favoriteId
        || aliceFavorites.front().moves != favorite.moves
        || !database.favoriteScores(bob).isEmpty()) {
        return 21;
    }
    const auto bobAfterDelete = database.completedGames(bob, 20);
    for (int index = 0; index < bobAfterDelete.size(); ++index) {
        if (bobAfterDelete[index].sequenceNumber != bobAfterDelete.size() - index) return 20;
    }
    return 0;
}
