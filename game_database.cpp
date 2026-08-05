#include "game_database.h"

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
        "CREATE TABLE IF NOT EXISTS games ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
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

        "CREATE INDEX IF NOT EXISTS idx_moves_game ON moves(game_id, ply)",
        "CREATE INDEX IF NOT EXISTS idx_analyses_game ON analyses(game_id, ply)",

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

qint64 GameDatabase::startGame(QString *errorMessage)
{
    QSqlQuery query(database_);
    query.prepare("INSERT INTO games(started_at, result) VALUES(?, 'ongoing')");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.lastInsertId().toLongLong();
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

GameDatabase::TrainingStats GameDatabase::trainingStats() const
{
    TrainingStats stats;
    QSqlQuery gameQuery(database_);
    if (gameQuery.exec("SELECT COUNT(*) FROM games") && gameQuery.next()) {
        stats.games = gameQuery.value(0).toInt();
    }

    QSqlQuery query(database_);
    query.prepare(
        "SELECT COUNT(*), COALESCE(AVG(a.score_loss), 0), "
        "SUM(CASE WHEN a.category = 'excellent' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'inaccuracy' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'mistake' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN a.category = 'blunder' THEN 1 ELSE 0 END) "
        "FROM analyses a JOIN moves m ON m.game_id = a.game_id AND m.ply = a.ply "
        "WHERE m.side = 'red'");
    if (query.exec() && query.next()) {
        stats.analyzedMoves = query.value(0).toInt();
        stats.averageLoss = query.value(1).toDouble();
        stats.excellentMoves = query.value(2).toInt();
        stats.inaccuracies = query.value(3).toInt();
        stats.mistakes = query.value(4).toInt();
        stats.blunders = query.value(5).toInt();
    }
    QSqlQuery coachingQuery(database_);
    if (coachingQuery.exec("SELECT COUNT(*) FROM coaching") && coachingQuery.next()) {
        stats.coachedMoves = coachingQuery.value(0).toInt();
    }
    return stats;
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
