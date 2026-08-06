#include "game_database.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>
#include <QVariant>

GameDatabase::GameDatabase()
    : connection_name_("xiangqi-training-" + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

GameDatabase::~GameDatabase()
{
    if (database_.isValid()) {
        database_.close();
    }
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_name_);
}

bool GameDatabase::open(QString *errorMessage)
{
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDirectory)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"无法创建数据目录：") + dataDirectory;
        }
        return false;
    }

    database_path_ = QDir(dataDirectory).filePath("xiangqi_training.db");
    database_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
    database_.setDatabaseName(database_path_);
    if (!database_.open()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }
    return executeSchema(errorMessage);
}

bool GameDatabase::executeSchema(QString *errorMessage)
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, "
        "created_at TEXT NOT NULL)",

        "INSERT OR IGNORE INTO users(id, name, created_at) "
        "VALUES(1, '默认用户', datetime('now', 'localtime'))",

        "CREATE TABLE IF NOT EXISTS app_state ("
        "key TEXT PRIMARY KEY, value TEXT NOT NULL)",

        "INSERT OR IGNORE INTO app_state(key, value) VALUES('selected_user_id', '1')",

        "CREATE TABLE IF NOT EXISTS games ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL DEFAULT 1, "
        "started_at TEXT NOT NULL, finished_at TEXT, result TEXT NOT NULL DEFAULT 'ongoing')",

        "CREATE TABLE IF NOT EXISTS moves ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, game_id INTEGER NOT NULL, ply INTEGER NOT NULL, "
        "side TEXT NOT NULL, from_row INTEGER NOT NULL, from_col INTEGER NOT NULL, "
        "to_row INTEGER NOT NULL, to_col INTEGER NOT NULL, moved_piece TEXT NOT NULL, "
        "captured_piece TEXT, thinking_time_ms INTEGER NOT NULL, board_before TEXT NOT NULL, "
        "board_after TEXT NOT NULL, result_after TEXT NOT NULL, "
        "UNIQUE(game_id, ply), FOREIGN KEY(game_id) REFERENCES games(id))",

        "CREATE TABLE IF NOT EXISTS analyses ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, game_id INTEGER NOT NULL, ply INTEGER NOT NULL, "
        "actual_move TEXT NOT NULL, best_move TEXT NOT NULL, best_score INTEGER NOT NULL, "
        "actual_score INTEGER NOT NULL, score_loss INTEGER NOT NULL, category TEXT NOT NULL, "
        "principal_variation TEXT, analyzed_at TEXT NOT NULL, "
        "UNIQUE(game_id, ply), FOREIGN KEY(game_id) REFERENCES games(id))",

        "CREATE TABLE IF NOT EXISTS coaching ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, game_id INTEGER NOT NULL, ply INTEGER NOT NULL, "
        "model TEXT NOT NULL, diagnosis TEXT NOT NULL, evidence TEXT NOT NULL, "
        "training_task TEXT NOT NULL, reflection_question TEXT NOT NULL, created_at TEXT NOT NULL, "
        "UNIQUE(game_id, ply), FOREIGN KEY(game_id) REFERENCES games(id))",

        "CREATE TABLE IF NOT EXISTS game_reviews ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, game_id INTEGER NOT NULL UNIQUE, "
        "model TEXT NOT NULL, overview TEXT NOT NULL, turning_points TEXT NOT NULL, "
        "strengths TEXT NOT NULL, recurring_pattern TEXT NOT NULL, "
        "training_plan TEXT NOT NULL, reflection_question TEXT NOT NULL, "
        "created_at TEXT NOT NULL, FOREIGN KEY(game_id) REFERENCES games(id))",

        "CREATE TABLE IF NOT EXISTS training_positions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, source_game_id INTEGER NOT NULL, "
        "source_ply INTEGER NOT NULL, board TEXT NOT NULL, best_move TEXT NOT NULL, "
        "actual_move TEXT NOT NULL, score_loss INTEGER NOT NULL, category TEXT NOT NULL, "
        "principal_variation TEXT, theme TEXT NOT NULL, mastery INTEGER NOT NULL DEFAULT 0, "
        "next_review_at TEXT, created_at TEXT NOT NULL, "
        "UNIQUE(source_game_id, source_ply))",

        "CREATE TABLE IF NOT EXISTS training_attempts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, training_position_id INTEGER NOT NULL, "
        "attempted_move TEXT NOT NULL, correct INTEGER NOT NULL, "
        "thinking_time_ms INTEGER NOT NULL, attempted_at TEXT NOT NULL, "
        "FOREIGN KEY(training_position_id) REFERENCES training_positions(id))",

        "CREATE TABLE IF NOT EXISTS profile_reports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, "
        "through_games INTEGER NOT NULL, summary TEXT NOT NULL, generated_at TEXT NOT NULL, "
        "UNIQUE(user_id, through_games), FOREIGN KEY(user_id) REFERENCES users(id))",

        "CREATE INDEX IF NOT EXISTS idx_moves_game ON moves(game_id, ply)",
        "CREATE INDEX IF NOT EXISTS idx_analyses_game ON analyses(game_id, ply)",
        "CREATE INDEX IF NOT EXISTS idx_game_reviews_game ON game_reviews(game_id)",
        "CREATE INDEX IF NOT EXISTS idx_training_due ON training_positions(next_review_at, mastery)",
        "CREATE INDEX IF NOT EXISTS idx_training_attempts_position "
        "ON training_attempts(training_position_id)",

        // Repair records produced by the previous mate-score conversion.
        "UPDATE analyses SET actual_score = best_score, score_loss = 0, category = 'excellent' "
        "WHERE actual_move = best_move AND score_loss > 0",
        "UPDATE analyses SET score_loss = MIN(30, MAX(0, best_score - actual_score)), "
        "category = 'excellent' WHERE actual_move <> best_move "
        "AND best_score > 90000 AND actual_score > 90000",
        "UPDATE analyses SET score_loss = 100, category = 'mistake' "
        "WHERE actual_move <> best_move AND best_score > 90000 "
        "AND actual_score > 500 AND actual_score <= 90000",
        "UPDATE analyses SET score_loss = 180, category = 'mistake' "
        "WHERE actual_move <> best_move AND best_score > 90000 "
        "AND actual_score > 0 AND actual_score <= 500",
        "DELETE FROM coaching WHERE EXISTS (SELECT 1 FROM analyses a "
        "WHERE a.game_id = coaching.game_id AND a.ply = coaching.ply "
        "AND (a.actual_move = a.best_move OR "
        "(a.best_score > 90000 AND a.actual_score > 0)))"
    };

    for (const QString &statement : statements) {
        QSqlQuery query(database_);
        if (!query.exec(statement)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    bool hasUserId = false;
    QSqlQuery columns(database_);
    if (!columns.exec("PRAGMA table_info(games)")) {
        if (errorMessage) *errorMessage = columns.lastError().text();
        return false;
    }
    while (columns.next()) {
        if (columns.value(1).toString() == "user_id") {
            hasUserId = true;
            break;
        }
    }
    if (!hasUserId) {
        QSqlQuery migration(database_);
        if (!migration.exec("ALTER TABLE games ADD COLUMN user_id INTEGER NOT NULL DEFAULT 1")) {
            if (errorMessage) *errorMessage = migration.lastError().text();
            return false;
        }
    }
    QSqlQuery index(database_);
    if (!index.exec("CREATE INDEX IF NOT EXISTS idx_games_user ON games(user_id, id)")) {
        if (errorMessage) *errorMessage = index.lastError().text();
        return false;
    }
    return true;
}

bool GameDatabase::recordCoaching(qint64 gameId, int ply, const QString &model,
                                  const QString &diagnosis, const QString &evidence,
                                  const QString &trainingTask,
                                  const QString &reflectionQuestion,
                                  QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare(
        "INSERT OR REPLACE INTO coaching(game_id, ply, model, diagnosis, evidence, "
        "training_task, reflection_question, created_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(ply);
    query.addBindValue(model);
    query.addBindValue(diagnosis);
    query.addBindValue(evidence);
    query.addBindValue(trainingTask);
    query.addBindValue(reflectionQuestion);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool GameDatabase::buildGameReviewContext(qint64 gameId,
                                          GameReviewContext *context,
                                          QString *errorMessage) const
{
    if (!context) {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"整盘复盘上下文不能为空");
        return false;
    }

    QSqlQuery game(database_);
    game.prepare("SELECT user_id, result FROM games WHERE id = ?");
    game.addBindValue(gameId);
    if (!game.exec() || !game.next()) {
        if (errorMessage) {
            *errorMessage = game.lastError().isValid()
                ? game.lastError().text() : QString::fromUtf8(u8"找不到对局");
        }
        return false;
    }
    const QString result = game.value(1).toString();
    if (result == "ongoing" || result == "abandoned") {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"只有已完成的有效对局才能整盘复盘");
        return false;
    }

    GameReviewContext built;
    built.gameId = gameId;
    built.userId = game.value(0).toLongLong();
    built.result = result;

    struct PhaseData { int count = 0; int totalLoss = 0; };
    PhaseData opening, middle, ending;
    QStringList transcript;
    QSqlQuery moves(database_);
    moves.prepare(
        "SELECT m.ply, m.side, m.from_row, m.from_col, m.to_row, m.to_col, "
        "m.moved_piece, m.captured_piece, m.thinking_time_ms, "
        "a.score_loss, a.category "
        "FROM moves m LEFT JOIN analyses a ON a.game_id=m.game_id AND a.ply=m.ply "
        "WHERE m.game_id=? ORDER BY m.ply");
    moves.addBindValue(gameId);
    if (!moves.exec()) {
        if (errorMessage) *errorMessage = moves.lastError().text();
        return false;
    }
    qint64 totalRedThinkingTime = 0;
    while (moves.next()) {
        const int ply = moves.value(0).toInt();
        const QString side = moves.value(1).toString();
        auto square = [](int row, int col) {
            return QString(QChar('a' + col)) + QChar('9' - row);
        };
        const QString uci = square(moves.value(2).toInt(), moves.value(3).toInt())
                            + square(moves.value(4).toInt(), moves.value(5).toInt());
        const QString captured = moves.value(7).toString();
        const qint64 thinkingTime = moves.value(8).toLongLong();
        transcript.push_back(QString::fromUtf8(u8"%1. %2 %3 %4%5（%6 秒）")
            .arg(ply)
            .arg(side == "red" ? QString::fromUtf8(u8"红") : QString::fromUtf8(u8"黑"))
            .arg(moves.value(6).toString(), uci,
                 captured.isEmpty() ? QString() : QString::fromUtf8(u8" 吃") + captured)
            .arg(thinkingTime / 1000.0, 0, 'f', 1));
        ++built.totalMoves;
        if (side != "red") continue;
        ++built.redMoves;
        totalRedThinkingTime += thinkingTime;
        if (moves.value(9).isNull()) continue;
        const int loss = std::min(300, std::max(0, moves.value(9).toInt()));
        const QString category = moves.value(10).toString();
        ++built.analyzedMoves;
        built.averageLoss += loss;
        if (category == "mistake") ++built.mistakes;
        if (category == "blunder") ++built.blunders;
        PhaseData *phase = ply <= 20 ? &opening : (ply <= 60 ? &middle : &ending);
        ++phase->count;
        phase->totalLoss += loss;
    }
    if (built.analyzedMoves > 0) built.averageLoss /= built.analyzedMoves;
    if (built.redMoves > 0) {
        built.averageThinkingTimeMs = static_cast<double>(totalRedThinkingTime) / built.redMoves;
    }
    built.moveTranscript = transcript.join('\n');

    auto phaseText = [](const QString &name, const PhaseData &phase) {
        return phase.count == 0
            ? name + QString::fromUtf8(u8"：没有可用分析")
            : QString::fromUtf8(u8"%1：分析 %2 步，平均损失 %3")
                  .arg(name).arg(phase.count)
                  .arg(static_cast<double>(phase.totalLoss) / phase.count, 0, 'f', 1);
    };
    built.phaseSummary = QStringList{
        phaseText(QString::fromUtf8(u8"开局（1～20）"), opening),
        phaseText(QString::fromUtf8(u8"中局（21～60）"), middle),
        phaseText(QString::fromUtf8(u8"残局（61 以后）"), ending)
    }.join('\n');

    QSqlQuery moments(database_);
    moments.prepare(
        "SELECT a.ply, a.actual_move, a.best_move, a.best_score, a.actual_score, "
        "MIN(a.score_loss,300), a.category, a.principal_variation, m.board_before "
        "FROM analyses a JOIN moves m ON m.game_id=a.game_id AND m.ply=a.ply "
        "WHERE a.game_id=? AND a.score_loss>30 "
        "ORDER BY MIN(a.score_loss,300) DESC, a.ply ASC LIMIT 5");
    moments.addBindValue(gameId);
    if (!moments.exec()) {
        if (errorMessage) *errorMessage = moments.lastError().text();
        return false;
    }
    QStringList keyMoments;
    while (moments.next()) {
        keyMoments.push_back(QString::fromUtf8(
            u8"第 %1 步：实际 %2，推荐 %3，评分 %4→%5，损失 %6，等级 %7；"
            u8"推荐变化：%8；走棋前局面：%9")
            .arg(moments.value(0).toInt())
            .arg(moments.value(1).toString(), moments.value(2).toString())
            .arg(moments.value(3).toInt()).arg(moments.value(4).toInt())
            .arg(moments.value(5).toInt())
            .arg(moments.value(6).toString(), moments.value(7).toString(),
                 moments.value(8).toString()));
    }
    built.keyMoments = keyMoments.isEmpty()
        ? QString::fromUtf8(u8"没有局面损失超过 30 的关键失误。")
        : keyMoments.join('\n');
    *context = built;
    return true;
}

bool GameDatabase::recordGameReview(const GameReview &review,
                                    QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare(
        "INSERT OR REPLACE INTO game_reviews(game_id, model, overview, turning_points, "
        "strengths, recurring_pattern, training_plan, reflection_question, created_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(review.gameId);
    query.addBindValue(review.model);
    query.addBindValue(review.overview);
    query.addBindValue(review.turningPoints);
    query.addBindValue(review.strengths);
    query.addBindValue(review.recurringPattern);
    query.addBindValue(review.trainingPlan);
    query.addBindValue(review.reflectionQuestion);
    query.addBindValue(review.createdAt.isEmpty()
                           ? QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                           : review.createdAt);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool GameDatabase::hasGameReview(qint64 gameId) const
{
    QSqlQuery query(database_);
    query.prepare("SELECT 1 FROM game_reviews WHERE game_id=?");
    query.addBindValue(gameId);
    return query.exec() && query.next();
}

GameDatabase::GameReview GameDatabase::gameReview(qint64 gameId) const
{
    GameReview review;
    QSqlQuery query(database_);
    query.prepare(
        "SELECT game_id, model, overview, turning_points, strengths, recurring_pattern, "
        "training_plan, reflection_question, created_at FROM game_reviews WHERE game_id=?");
    query.addBindValue(gameId);
    if (query.exec() && query.next()) {
        review.gameId = query.value(0).toLongLong();
        review.model = query.value(1).toString();
        review.overview = query.value(2).toString();
        review.turningPoints = query.value(3).toString();
        review.strengths = query.value(4).toString();
        review.recurringPattern = query.value(5).toString();
        review.trainingPlan = query.value(6).toString();
        review.reflectionQuestion = query.value(7).toString();
        review.createdAt = query.value(8).toString();
    }
    return review;
}

QVector<GameDatabase::User> GameDatabase::users() const
{
    QVector<User> result;
    QSqlQuery query(database_);
    if (!query.exec("SELECT id, name, created_at FROM users ORDER BY id")) {
        return result;
    }
    while (query.next()) {
        result.push_back(User{query.value(0).toLongLong(),
                              query.value(1).toString(),
                              query.value(2).toString()});
    }
    return result;
}

qint64 GameDatabase::createUser(const QString &name, QString *errorMessage)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"用户名不能为空");
        return -1;
    }
    QSqlQuery query(database_);
    query.prepare("INSERT INTO users(name, created_at) VALUES(?, ?)");
    query.addBindValue(cleanName);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

qint64 GameDatabase::selectedUserId() const
{
    QSqlQuery query(database_);
    if (query.exec("SELECT value FROM app_state WHERE key = 'selected_user_id'") &&
        query.next()) {
        return query.value(0).toLongLong();
    }
    return 1;
}

bool GameDatabase::setSelectedUserId(qint64 userId, QString *errorMessage)
{
    QSqlQuery check(database_);
    check.prepare("SELECT 1 FROM users WHERE id = ?");
    check.addBindValue(userId);
    if (!check.exec() || !check.next()) {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"用户不存在");
        return false;
    }
    QSqlQuery query(database_);
    query.prepare("INSERT OR REPLACE INTO app_state(key, value) "
                  "VALUES('selected_user_id', ?)");
    query.addBindValue(QString::number(userId));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

int GameDatabase::generateTrainingPositions(qint64 userId, QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare(
        "INSERT OR IGNORE INTO training_positions("
        "source_game_id, source_ply, board, best_move, actual_move, score_loss, category, "
        "principal_variation, theme, mastery, next_review_at, created_at) "
        "SELECT a.game_id, a.ply, m.board_before, a.best_move, a.actual_move, "
        "a.score_loss, a.category, a.principal_variation, "
        "CASE WHEN a.best_score > 90000 THEN '寻找将杀' "
        "WHEN a.score_loss > 200 THEN '防止严重失误' "
        "WHEN a.score_loss > 80 THEN '候选着比较' "
        "ELSE '局面优化' END, 0, ?, ? "
        "FROM analyses a JOIN moves m ON m.game_id = a.game_id AND m.ply = a.ply "
        "JOIN games g ON g.id = a.game_id "
        "WHERE g.user_id = ? AND m.side = 'red' "
        "AND a.score_loss > 30 AND a.best_move <> ''");
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.numRowsAffected();
}

QVector<GameDatabase::TrainingPosition> GameDatabase::dueTrainingPositions(qint64 userId,
                                                                            int limit) const
{
    QVector<TrainingPosition> positions;
    QSqlQuery query(database_);
    query.prepare(
        "SELECT p.id, p.source_game_id, p.source_ply, p.board, p.best_move, "
        "p.actual_move, p.score_loss, p.category, p.principal_variation, p.theme, "
        "p.mastery, COUNT(t.id), COALESCE(SUM(t.correct), 0) "
        "FROM training_positions p JOIN games g ON g.id = p.source_game_id "
        "LEFT JOIN training_attempts t "
        "ON t.training_position_id = p.id "
        "WHERE g.user_id = ? AND (p.next_review_at IS NULL OR p.next_review_at <= ?) "
        "GROUP BY p.id ORDER BY p.mastery ASC, p.score_loss DESC, p.id ASC LIMIT ?");
    query.addBindValue(userId);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    query.addBindValue(std::max(1, limit));
    if (!query.exec()) {
        return positions;
    }
    while (query.next()) {
        TrainingPosition position;
        position.id = query.value(0).toLongLong();
        position.sourceGameId = query.value(1).toLongLong();
        position.sourcePly = query.value(2).toInt();
        position.board = query.value(3).toString();
        position.bestMove = query.value(4).toString();
        position.actualMove = query.value(5).toString();
        position.scoreLoss = query.value(6).toInt();
        position.category = query.value(7).toString();
        position.principalVariation = query.value(8).toString();
        position.theme = query.value(9).toString();
        position.mastery = query.value(10).toInt();
        position.attempts = query.value(11).toInt();
        position.correctAttempts = query.value(12).toInt();
        positions.push_back(position);
    }
    return positions;
}

bool GameDatabase::recordTrainingAttempt(qint64 positionId,
                                         const QString &attemptedMove,
                                         bool correct,
                                         qint64 thinkingTimeMs,
                                         QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage) *errorMessage = database_.lastError().text();
        return false;
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    QSqlQuery insert(database_);
    insert.prepare(
        "INSERT INTO training_attempts(training_position_id, attempted_move, correct, "
        "thinking_time_ms, attempted_at) VALUES(?, ?, ?, ?, ?)");
    insert.addBindValue(positionId);
    insert.addBindValue(attemptedMove);
    insert.addBindValue(correct ? 1 : 0);
    insert.addBindValue(std::max<qint64>(0, thinkingTimeMs));
    insert.addBindValue(now);
    if (!insert.exec()) {
        database_.rollback();
        if (errorMessage) *errorMessage = insert.lastError().text();
        return false;
    }

    QSqlQuery masteryQuery(database_);
    masteryQuery.prepare("SELECT mastery FROM training_positions WHERE id = ?");
    masteryQuery.addBindValue(positionId);
    if (!masteryQuery.exec() || !masteryQuery.next()) {
        database_.rollback();
        if (errorMessage) *errorMessage = masteryQuery.lastError().text();
        return false;
    }
    const int oldMastery = masteryQuery.value(0).toInt();
    const int newMastery = correct ? std::min(5, oldMastery + 1)
                                   : std::max(0, oldMastery - 1);
    int reviewDays = 0;
    if (correct) {
        static const int intervals[] = {0, 1, 3, 7, 14, 30};
        reviewDays = intervals[newMastery];
    }
    const QString nextReview = QDateTime::currentDateTime()
                                   .addDays(reviewDays)
                                   .toString(Qt::ISODateWithMs);

    QSqlQuery update(database_);
    update.prepare(
        "UPDATE training_positions SET mastery = ?, next_review_at = ? WHERE id = ?");
    update.addBindValue(newMastery);
    update.addBindValue(nextReview);
    update.addBindValue(positionId);
    if (!update.exec() || !database_.commit()) {
        database_.rollback();
        if (errorMessage) *errorMessage = update.lastError().text();
        return false;
    }
    return true;
}

GameDatabase::TrainingSummary GameDatabase::trainingSummary(qint64 userId) const
{
    TrainingSummary summary;
    QSqlQuery positions(database_);
    positions.prepare(
        "SELECT COUNT(*), SUM(CASE WHEN next_review_at IS NULL OR next_review_at <= ? "
        "THEN 1 ELSE 0 END) FROM training_positions p "
        "JOIN games g ON g.id = p.source_game_id WHERE g.user_id = ?");
    positions.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    positions.addBindValue(userId);
    if (positions.exec() && positions.next()) {
        summary.positions = positions.value(0).toInt();
        summary.due = positions.value(1).toInt();
    }
    QSqlQuery attempts(database_);
    attempts.prepare(
        "SELECT COUNT(*), COALESCE(SUM(t.correct), 0) FROM training_attempts t "
        "JOIN training_positions p ON p.id = t.training_position_id "
        "JOIN games g ON g.id = p.source_game_id WHERE g.user_id = ?");
    attempts.addBindValue(userId);
    if (attempts.exec() && attempts.next()) {
        summary.attempts = attempts.value(0).toInt();
        summary.correctAttempts = attempts.value(1).toInt();
    }
    return summary;
}

qint64 GameDatabase::startGame(qint64 userId, QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("INSERT INTO games(user_id, started_at, result) VALUES(?, ?, 'ongoing')");
    query.addBindValue(userId);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool GameDatabase::abandonGame(qint64 gameId, QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("UPDATE games SET finished_at = ?, result = 'abandoned' "
                  "WHERE id = ? AND result = 'ongoing'");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    query.addBindValue(gameId);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

bool GameDatabase::recordMove(qint64 gameId, const XiangqiGame::MoveRecord &move,
                              QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare(
        "INSERT INTO moves(game_id, ply, side, from_row, from_col, to_row, to_col, "
        "moved_piece, captured_piece, thinking_time_ms, board_before, board_after, result_after) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(move.ply);
    query.addBindValue(sideName(move.side));
    query.addBindValue(move.fromRow);
    query.addBindValue(move.fromCol);
    query.addBindValue(move.toRow);
    query.addBindValue(move.toCol);
    query.addBindValue(pieceCode(move.movedPiece));
    query.addBindValue(pieceCode(move.capturedPiece));
    query.addBindValue(static_cast<qlonglong>(move.thinkingTimeMs));
    query.addBindValue(QString::fromStdString(move.boardBefore));
    query.addBindValue(QString::fromStdString(move.boardAfter));
    query.addBindValue(resultName(move.resultAfter));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool GameDatabase::finishGame(qint64 gameId, XiangqiGame::GameResult result,
                              QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("UPDATE games SET finished_at = ?, result = ? WHERE id = ?");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    query.addBindValue(resultName(result));
    query.addBindValue(gameId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool GameDatabase::truncateGame(qint64 gameId, int lastKeptPly,
                                QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery reviewQuery(database_);
    reviewQuery.prepare("DELETE FROM game_reviews WHERE game_id = ?");
    reviewQuery.addBindValue(gameId);
    if (!reviewQuery.exec()) {
        database_.rollback();
        if (errorMessage) *errorMessage = reviewQuery.lastError().text();
        return false;
    }

    QSqlQuery attemptQuery(database_);
    attemptQuery.prepare(
        "DELETE FROM training_attempts WHERE training_position_id IN ("
        "SELECT id FROM training_positions WHERE source_game_id = ? AND source_ply > ?)");
    attemptQuery.addBindValue(gameId);
    attemptQuery.addBindValue(lastKeptPly);
    if (!attemptQuery.exec()) {
        database_.rollback();
        if (errorMessage) *errorMessage = attemptQuery.lastError().text();
        return false;
    }

    QSqlQuery positionQuery(database_);
    positionQuery.prepare(
        "DELETE FROM training_positions WHERE source_game_id = ? AND source_ply > ?");
    positionQuery.addBindValue(gameId);
    positionQuery.addBindValue(lastKeptPly);
    if (!positionQuery.exec()) {
        database_.rollback();
        if (errorMessage) *errorMessage = positionQuery.lastError().text();
        return false;
    }

    const QStringList tables = {"coaching", "analyses", "moves"};
    for (const QString &table : tables) {
        QSqlQuery query(database_);
        query.prepare(QString("DELETE FROM %1 WHERE game_id = ? AND ply > ?").arg(table));
        query.addBindValue(gameId);
        query.addBindValue(lastKeptPly);
        if (!query.exec()) {
            database_.rollback();
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    QSqlQuery gameQuery(database_);
    gameQuery.prepare("UPDATE games SET finished_at = NULL, result = 'ongoing' WHERE id = ?");
    gameQuery.addBindValue(gameId);
    if (!gameQuery.exec() || !database_.commit()) {
        database_.rollback();
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }
    return true;
}

bool GameDatabase::recordAnalysis(qint64 gameId, int ply, const QString &actualMove,
                                  const QString &bestMove, int bestScore, int actualScore,
                                  int scoreLoss, const QString &category,
                                  const QString &principalVariation,
                                  QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare(
        "INSERT OR REPLACE INTO analyses(game_id, ply, actual_move, best_move, best_score, "
        "actual_score, score_loss, category, principal_variation, analyzed_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(ply);
    query.addBindValue(actualMove);
    query.addBindValue(bestMove);
    query.addBindValue(bestScore);
    query.addBindValue(actualScore);
    query.addBindValue(scoreLoss);
    query.addBindValue(category);
    query.addBindValue(principalVariation);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

GameDatabase::TrainingStats GameDatabase::trainingStats(qint64 userId) const
{
    TrainingStats stats;
    QSqlQuery gameQuery(database_);
    gameQuery.prepare("SELECT COUNT(*) FROM games WHERE user_id = ? "
                      "AND result NOT IN ('ongoing', 'abandoned')");
    gameQuery.addBindValue(userId);
    if (gameQuery.exec() && gameQuery.next()) {
        stats.games = gameQuery.value(0).toInt();
    }

    QSqlQuery query(database_);
    query.prepare(
        "SELECT COUNT(*), COALESCE(AVG(MIN(a.score_loss, 300)), 0), "
        "SUM(CASE WHEN a.category = 'excellent' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'inaccuracy' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'mistake' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'blunder' THEN 1 ELSE 0 END) "
        "FROM analyses a JOIN moves m ON m.game_id = a.game_id AND m.ply = a.ply "
        "JOIN games g ON g.id = a.game_id WHERE g.user_id = ? AND m.side = 'red'");
    query.addBindValue(userId);
    if (query.exec() && query.next()) {
        stats.analyzedMoves = query.value(0).toInt();
        stats.averageLoss = query.value(1).toDouble();
        stats.excellentMoves = query.value(2).toInt();
        stats.inaccuracies = query.value(3).toInt();
        stats.mistakes = query.value(4).toInt();
        stats.blunders = query.value(5).toInt();
    }
    QSqlQuery coachingQuery(database_);
    coachingQuery.prepare(
        "SELECT COUNT(*) FROM coaching c JOIN games g ON g.id = c.game_id "
        "WHERE g.user_id = ?");
    coachingQuery.addBindValue(userId);
    if (coachingQuery.exec() && coachingQuery.next()) {
        stats.coachedMoves = coachingQuery.value(0).toInt();
    }
    return stats;
}

GameDatabase::UserProfile GameDatabase::userProfile(qint64 userId) const
{
    UserProfile profile;
    QSqlQuery games(database_);
    games.prepare(
        "SELECT COUNT(*), "
        "COALESCE(SUM(CASE WHEN result='red_wins' THEN 1 ELSE 0 END),0), "
        "COALESCE(SUM(CASE WHEN result='black_wins' THEN 1 ELSE 0 END),0), "
        "COALESCE(SUM(CASE WHEN result='draw' THEN 1 ELSE 0 END),0) "
        "FROM games WHERE user_id=? AND result NOT IN ('ongoing','abandoned')");
    games.addBindValue(userId);
    if (games.exec() && games.next()) {
        profile.completedGames = games.value(0).toInt();
        profile.wins = games.value(1).toInt();
        profile.losses = games.value(2).toInt();
        profile.draws = games.value(3).toInt();
    }

    QSqlQuery analysis(database_);
    analysis.prepare(
        "SELECT COUNT(*), COALESCE(AVG(MIN(a.score_loss,300)),0), "
        "COALESCE(SUM(CASE WHEN a.category='blunder' THEN 1 ELSE 0 END),0), "
        "COALESCE(AVG(m.thinking_time_ms),0), "
        "COALESCE(SUM(CASE WHEN a.category='mistake' THEN 1 ELSE 0 END),0), "
        "COALESCE(SUM(CASE WHEN a.category='inaccuracy' THEN 1 ELSE 0 END),0) "
        "FROM analyses a JOIN moves m ON m.game_id=a.game_id AND m.ply=a.ply "
        "JOIN games g ON g.id=a.game_id WHERE g.user_id=? AND m.side='red'");
    analysis.addBindValue(userId);
    int mistakes = 0;
    int inaccuracies = 0;
    if (analysis.exec() && analysis.next()) {
        profile.analyzedMoves = analysis.value(0).toInt();
        profile.averageLoss = analysis.value(1).toDouble();
        profile.blunders = analysis.value(2).toInt();
        profile.averageThinkingTimeMs = analysis.value(3).toDouble();
        mistakes = analysis.value(4).toInt();
        inaccuracies = analysis.value(5).toInt();
    }

    const TrainingSummary training = trainingSummary(userId);
    profile.trainingAttempts = training.attempts;
    profile.trainingCorrect = training.correctAttempts;
    if (profile.blunders > 0) {
        profile.mainWeakness = QString::fromUtf8(u8"减少严重失误，落子前检查对方的将军、吃子和直接威胁");
    } else if (mistakes > inaccuracies) {
        profile.mainWeakness = QString::fromUtf8(u8"加强候选着比较，至少计算两个可选方案");
    } else if (profile.analyzedMoves > 0) {
        profile.mainWeakness = QString::fromUtf8(u8"提高局面优化能力，并保持稳定思考节奏");
    } else {
        profile.mainWeakness = QString::fromUtf8(u8"数据不足，请先完成更多有效对局");
    }
    return profile;
}

GameDatabase::ProfileReport GameDatabase::generateMilestoneReport(
    qint64 userId, bool *created, QString *errorMessage)
{
    if (created) *created = false;
    ProfileReport report;

    QSqlQuery countQuery(database_);
    countQuery.prepare("SELECT COUNT(*) FROM games WHERE user_id=? "
                       "AND result NOT IN ('ongoing','abandoned')");
    countQuery.addBindValue(userId);
    if (!countQuery.exec() || !countQuery.next()) {
        if (errorMessage) *errorMessage = countQuery.lastError().text();
        return report;
    }
    const int completed = countQuery.value(0).toInt();
    const int milestone = (completed / 10) * 10;
    if (milestone < 10) {
        return report;
    }

    QSqlQuery existing(database_);
    existing.prepare("SELECT id, summary, generated_at FROM profile_reports "
                     "WHERE user_id=? AND through_games=?");
    existing.addBindValue(userId);
    existing.addBindValue(milestone);
    if (existing.exec() && existing.next()) {
        report.id = existing.value(0).toLongLong();
        report.userId = userId;
        report.throughGames = milestone;
        report.summary = existing.value(1).toString();
        report.generatedAt = existing.value(2).toString();
        return report;
    }

    const int offset = milestone - 10;
    QSqlQuery games(database_);
    games.prepare(
        "SELECT COALESCE(SUM(result='red_wins'),0), "
        "COALESCE(SUM(result='black_wins'),0), COALESCE(SUM(result='draw'),0) "
        "FROM (SELECT result FROM games WHERE user_id=? "
        "AND result NOT IN ('ongoing','abandoned') ORDER BY id LIMIT 10 OFFSET ?)");
    games.addBindValue(userId);
    games.addBindValue(offset);
    int wins = 0, losses = 0, draws = 0;
    if (games.exec() && games.next()) {
        wins = games.value(0).toInt();
        losses = games.value(1).toInt();
        draws = games.value(2).toInt();
    }

    QSqlQuery analysis(database_);
    analysis.prepare(
        "SELECT COUNT(*), COALESCE(AVG(MIN(a.score_loss,300)),0), "
        "COALESCE(SUM(a.category='blunder'),0), "
        "COALESCE(SUM(a.category='mistake'),0), "
        "COALESCE(AVG(m.thinking_time_ms),0) "
        "FROM analyses a JOIN moves m ON m.game_id=a.game_id AND m.ply=a.ply "
        "WHERE m.side='red' AND a.game_id IN (SELECT id FROM games WHERE user_id=? "
        "AND result NOT IN ('ongoing','abandoned') ORDER BY id LIMIT 10 OFFSET ?)");
    analysis.addBindValue(userId);
    analysis.addBindValue(offset);
    int analyzed = 0, blunders = 0, mistakes = 0;
    double averageLoss = 0.0, averageTime = 0.0;
    if (analysis.exec() && analysis.next()) {
        analyzed = analysis.value(0).toInt();
        averageLoss = analysis.value(1).toDouble();
        blunders = analysis.value(2).toInt();
        mistakes = analysis.value(3).toInt();
        averageTime = analysis.value(4).toDouble();
    }

    QString advice;
    if (blunders >= 3) {
        advice = QString::fromUtf8(u8"下一阶段重点减少严重失误：每次落子前固定检查对方的将军、吃子和直接威胁。手动完成个人错题中的“防止严重失误”训练。");
    } else if (mistakes >= 4) {
        advice = QString::fromUtf8(u8"严重失误已经得到控制，下一阶段应加强候选着比较。每步至少列出两个候选着，再比较对方最强回应。");
    } else if (analyzed > 0 && averageTime < 3000.0) {
        advice = QString::fromUtf8(u8"平均思考时间偏短。建议建立稳定的落子检查流程，避免仅凭第一感觉行动。");
    } else {
        advice = QString::fromUtf8(u8"当前发挥较稳定。继续完成间隔复习，并重点训练局面优化和连续计算能力。");
    }

    const TrainingSummary training = trainingSummary(userId);
    const double accuracy = training.attempts > 0
                                ? 100.0 * training.correctAttempts / training.attempts : 0.0;
    report.userId = userId;
    report.throughGames = milestone;
    report.generatedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    report.summary = QString::fromUtf8(
        u8"第 %1 盘阶段总结\n\n最近十盘：%2 胜、%3 负、%4 和。\n"
        u8"共分析 %5 个红方走法，平均局面损失 %6；明显失误 %7 次，严重失误 %8 次。\n"
        u8"平均每步思考 %9 秒。专项训练累计正确率 %10%。\n\n训练建议：%11")
        .arg(milestone).arg(wins).arg(losses).arg(draws).arg(analyzed)
        .arg(averageLoss, 0, 'f', 1).arg(mistakes).arg(blunders)
        .arg(averageTime / 1000.0, 0, 'f', 1).arg(accuracy, 0, 'f', 1).arg(advice);

    QSqlQuery insert(database_);
    insert.prepare("INSERT INTO profile_reports(user_id, through_games, summary, generated_at) "
                   "VALUES(?, ?, ?, ?)");
    insert.addBindValue(userId);
    insert.addBindValue(milestone);
    insert.addBindValue(report.summary);
    insert.addBindValue(report.generatedAt);
    if (!insert.exec()) {
        if (errorMessage) *errorMessage = insert.lastError().text();
        return ProfileReport{};
    }
    report.id = insert.lastInsertId().toLongLong();
    if (created) *created = true;
    return report;
}

QVector<GameDatabase::ProfileReport> GameDatabase::profileReports(qint64 userId) const
{
    QVector<ProfileReport> reports;
    QSqlQuery query(database_);
    query.prepare("SELECT id, through_games, summary, generated_at FROM profile_reports "
                  "WHERE user_id=? ORDER BY through_games DESC");
    query.addBindValue(userId);
    if (!query.exec()) return reports;
    while (query.next()) {
        ProfileReport report;
        report.id = query.value(0).toLongLong();
        report.userId = userId;
        report.throughGames = query.value(1).toInt();
        report.summary = query.value(2).toString();
        report.generatedAt = query.value(3).toString();
        reports.push_back(report);
    }
    return reports;
}

QVector<GameDatabase::GamePerformance> GameDatabase::recentGamePerformance(
    qint64 userId, int limit) const
{
    QVector<GamePerformance> performance;
    QSqlQuery query(database_);
    query.prepare(
        "SELECT g.id, g.result, COALESCE(AVG(MIN(a.score_loss,300)),0), "
        "COALESCE(SUM(CASE WHEN a.category='blunder' THEN 1 ELSE 0 END),0) "
        "FROM games g LEFT JOIN analyses a ON a.game_id=g.id "
        "WHERE g.user_id=? AND g.result NOT IN ('ongoing','abandoned') "
        "GROUP BY g.id ORDER BY g.id DESC LIMIT ?");
    query.addBindValue(userId);
    query.addBindValue(std::max(1, limit));
    if (!query.exec()) {
        return performance;
    }
    while (query.next()) {
        performance.push_back(GamePerformance{
            query.value(0).toLongLong(), query.value(1).toString(),
            query.value(2).toDouble(), query.value(3).toInt()});
    }
    std::reverse(performance.begin(), performance.end());
    return performance;
}

QString GameDatabase::databasePath() const
{
    return database_path_;
}

QString GameDatabase::sideName(XiangqiGame::Side side)
{
    return side == XiangqiGame::Side::Red ? "red" : "black";
}

QString GameDatabase::resultName(XiangqiGame::GameResult result)
{
    switch (result) {
    case XiangqiGame::GameResult::RedWins: return "red_wins";
    case XiangqiGame::GameResult::BlackWins: return "black_wins";
    case XiangqiGame::GameResult::Draw: return "draw";
    case XiangqiGame::GameResult::Ongoing: return "ongoing";
    }
    return "ongoing";
}

QString GameDatabase::pieceCode(const std::optional<XiangqiGame::Piece> &piece)
{
    if (!piece.has_value()) {
        return QString();
    }
    QChar code;
    switch (piece->type) {
    case XiangqiGame::PieceType::General: code = 'K'; break;
    case XiangqiGame::PieceType::Advisor: code = 'A'; break;
    case XiangqiGame::PieceType::Elephant: code = 'E'; break;
    case XiangqiGame::PieceType::Horse: code = 'H'; break;
    case XiangqiGame::PieceType::Rook: code = 'R'; break;
    case XiangqiGame::PieceType::Cannon: code = 'C'; break;
    case XiangqiGame::PieceType::Soldier: code = 'S'; break;
    }
    return piece->side == XiangqiGame::Side::Red ? QString(code)
                                                  : QString(code.toLower());
}

QString GameDatabase::pieceCode(const XiangqiGame::Piece &piece)
{
    return pieceCode(std::optional<XiangqiGame::Piece>(piece));
}
