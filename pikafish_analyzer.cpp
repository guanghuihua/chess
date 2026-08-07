#include "pikafish_analyzer.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTimer>

PikafishAnalyzer::PikafishAnalyzer(QObject *parent)
    : QObject(parent)
    , engine_path_(findEngineExecutable())
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &PikafishAnalyzer::handleOutput);
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        const QString error = QString::fromUtf8(process_.readAllStandardError()).trimmed();
        if (!error.isEmpty()) {
            emit statusChanged(QString::fromUtf8(u8"皮卡鱼：") + error, false);
        }
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        rejectPositionRequests(QString::fromUtf8(u8"Pikafish 启动失败：") + process_.errorString());
        state_ = State::Stopped;
        requests_.clear();
        position_requests_.clear();
        emit statusChanged(QString::fromUtf8(u8"皮卡鱼启动失败：") + process_.errorString(), false);
        emit analysisQueueDrained();
    });
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                rejectPositionRequests(QString::fromUtf8(u8"Pikafish 已停止，无法验证 AI 候选题。"));
                state_ = State::Stopped;
                requests_.clear();
                position_requests_.clear();
                emit statusChanged(QString::fromUtf8(u8"皮卡鱼已停止"), false);
                emit analysisQueueDrained();
            });

    QTimer::singleShot(0, this, &PikafishAnalyzer::startEngine);
}

PikafishAnalyzer::~PikafishAnalyzer()
{
    if (process_.state() != QProcess::NotRunning) {
        sendCommand("quit");
        process_.waitForFinished(1000);
        if (process_.state() != QProcess::NotRunning) {
            process_.kill();
            process_.waitForFinished(500);
        }
    }
}

void PikafishAnalyzer::analyzeMove(qint64 gameId, const XiangqiGame::MoveRecord &move)
{
    if (move.side != XiangqiGame::Side::Red) {
        return;
    }
    requests_.enqueue(Request{gameId, move});
    if (state_ == State::Stopped) {
        startEngine();
    } else if (state_ == State::Idle) {
        processNextRequest();
    }
}

bool PikafishAnalyzer::analyzeTrainingPosition(const QString &requestId, const QString &board,
                                               XiangqiGame::Side sideToMove,
                                               QString *errorMessage)
{
    XiangqiGame position;
    if (requestId.isEmpty() || !position.loadPosition(board.toStdString(), sideToMove)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"AI 候选题的局面编码不合法，已拒绝入题库。");
        }
        return false;
    }
    if (engine_path_.isEmpty()) {
        rejectPositionRequests(QString::fromUtf8(u8"未找到 Pikafish，无法验证 AI 候选题。"));
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(u8"未找到 Pikafish，无法验证 AI 候选题。");
        }
        return false;
    }
    position_requests_.enqueue(PositionRequest{requestId, board, sideToMove});
    if (state_ == State::Stopped) {
        startEngine();
    } else if (state_ == State::Idle) {
        processNextRequest();
    }
    return true;
}

bool PikafishAnalyzer::isAvailable() const
{
    return !engine_path_.isEmpty() && process_.state() == QProcess::Running;
}

bool PikafishAnalyzer::hasPendingAnalysis() const
{
    if (engine_path_.isEmpty()) {
        return false;
    }
    return !requests_.isEmpty()
           || !position_requests_.isEmpty()
           || state_ == State::WaitingForUci
           || state_ == State::WaitingForReady
           || state_ == State::AnalyzingBefore
           || state_ == State::AnalyzingAfter;
}

QString PikafishAnalyzer::enginePath() const
{
    return engine_path_;
}

void PikafishAnalyzer::startEngine()
{
    if (process_.state() != QProcess::NotRunning) {
        return;
    }
    if (engine_path_.isEmpty()) {
        emit statusChanged(QString::fromUtf8(u8"未找到皮卡鱼；对局仍会保存，但暂不自动复盘"), false);
        requests_.clear();
        emit analysisQueueDrained();
        return;
    }

    process_.setProgram(engine_path_);
    process_.setWorkingDirectory(QFileInfo(engine_path_).absolutePath());
    state_ = State::WaitingForUci;
    process_.start();
    if (!process_.waitForStarted(3000)) {
        rejectPositionRequests(QString::fromUtf8(u8"无法启动 Pikafish：") + process_.errorString());
        state_ = State::Stopped;
        requests_.clear();
        position_requests_.clear();
        emit statusChanged(QString::fromUtf8(u8"无法启动皮卡鱼：") + process_.errorString(), false);
        emit analysisQueueDrained();
        return;
    }
    sendCommand("uci");
}

void PikafishAnalyzer::rejectPositionRequests(const QString &errorMessage)
{
    if (state_ == State::AnalyzingPosition && !current_position_.requestId.isEmpty()) {
        PositionAnalysis failed;
        failed.requestId = current_position_.requestId;
        emit trainingPositionAnalyzed(failed, errorMessage);
    }
    while (!position_requests_.isEmpty()) {
        PositionAnalysis failed;
        failed.requestId = position_requests_.dequeue().requestId;
        emit trainingPositionAnalyzed(failed, errorMessage);
    }
    current_position_ = PositionRequest{};
}

void PikafishAnalyzer::handleOutput()
{
    output_buffer_ += QString::fromUtf8(process_.readAllStandardOutput());
    int newline = -1;
    while ((newline = output_buffer_.indexOf('\n')) >= 0) {
        const QString line = output_buffer_.left(newline).trimmed();
        output_buffer_.remove(0, newline + 1);
        if (!line.isEmpty()) {
            handleLine(line);
        }
    }
}

void PikafishAnalyzer::handleLine(const QString &line)
{
    if (line == "uciok" && state_ == State::WaitingForUci) {
        sendCommand("setoption name Threads value 2");
        sendCommand("setoption name Hash value 128");
        sendCommand("setoption name MultiPV value 1");
        sendCommand("isready");
        state_ = State::WaitingForReady;
        return;
    }
    if (line == "readyok" && state_ == State::WaitingForReady) {
        state_ = State::Idle;
        emit statusChanged(QString::fromUtf8(u8"皮卡鱼已就绪，红方每步将自动复盘"), true);
        processNextRequest();
        if (state_ == State::Idle) {
            emit analysisQueueDrained();
        }
        return;
    }

    if ((state_ == State::AnalyzingBefore || state_ == State::AnalyzingAfter) &&
        line.startsWith("info ")) {
        int score = latest_score_;
        QString pv = latest_pv_;
        if (parseScoreAndPv(line, &score, &pv)) {
            latest_score_ = score;
            if (!pv.isEmpty()) {
                latest_pv_ = pv;
            }
        }
        return;
    }

    if (!line.startsWith("bestmove ")) {
        return;
    }
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    if (state_ == State::AnalyzingBefore) {
        best_score_ = latest_score_;
        best_move_ = parts.size() >= 2 ? parts[1] : QString();
        best_pv_ = latest_pv_;
        beginAfterAnalysis();
    } else if (state_ == State::AnalyzingAfter) {
        finishCurrentAnalysis();
    } else if (state_ == State::AnalyzingPosition) {
        finishPositionAnalysis(parts.size() >= 2 ? parts.at(1) : QString());
    }
}

void PikafishAnalyzer::processNextRequest()
{
    if (state_ != State::Idle) {
        return;
    }
    if (!requests_.isEmpty()) {
        current_ = requests_.dequeue();
        beginBeforeAnalysis();
        return;
    }
    if (!position_requests_.isEmpty()) {
        current_position_ = position_requests_.dequeue();
        beginPositionAnalysis();
    }
}

void PikafishAnalyzer::beginPositionAnalysis()
{
    latest_score_ = 0;
    latest_pv_.clear();
    const QString fen = toFen(current_position_.board.toStdString(),
                              current_position_.sideToMove, 1);
    sendCommand("position fen " + fen);
    sendCommand("go movetime 900");
    state_ = State::AnalyzingPosition;
    emit statusChanged(QString::fromUtf8(u8"Pikafish 正在验证 AI 生成训练题……"), true);
}

void PikafishAnalyzer::finishPositionAnalysis(const QString &bestMove)
{
    PositionAnalysis result;
    result.requestId = current_position_.requestId;
    result.board = current_position_.board;
    result.bestMove = bestMove;
    result.rawPrincipalVariation = latest_pv_;
    result.score = latest_score_;
    result.sideToMove = current_position_.sideToMove;

    QString error;
    QStringList pv = result.rawPrincipalVariation.split(' ', Qt::SkipEmptyParts);
    if (pv.isEmpty()) {
        pv.push_back(result.bestMove);
    } else if (pv.front() != result.bestMove) {
        pv.push_front(result.bestMove);
    }
    result.rawPrincipalVariation = pv.join(' ');
    XiangqiGame verifier;
    const auto decodeAndApply = [&verifier](const QString &move) {
        if (move.size() != 4) return false;
        const int fromCol = move.at(0).unicode() - QChar('a').unicode();
        const int fromRow = QChar('9').unicode() - move.at(1).unicode();
        const int toCol = move.at(2).unicode() - QChar('a').unicode();
        const int toRow = QChar('9').unicode() - move.at(3).unicode();
        return XiangqiGame::inBounds(fromRow, fromCol)
               && XiangqiGame::inBounds(toRow, toCol)
               && verifier.isLegalMove(fromRow, fromCol, toRow, toCol)
               && verifier.move(fromRow, fromCol, toRow, toCol);
    };
    if (!verifier.loadPosition(result.board.toStdString(), result.sideToMove)
        || !decodeAndApply(result.bestMove)) {
        error = QString::fromUtf8(u8"Pikafish 未返回可验证的最佳着，AI 候选题已丢弃。");
    } else {
        for (int index = 1; index < pv.size(); ++index) {
            if (!decodeAndApply(pv.at(index))) {
                error = QString::fromUtf8(u8"Pikafish 主变无法通过规则校验，AI 候选题已丢弃。");
                break;
            }
        }
    }
    emit trainingPositionAnalyzed(result, error);
    state_ = State::Idle;
    processNextRequest();
    if (state_ == State::Idle) {
        emit statusChanged(QString::fromUtf8(u8"Pikafish 已就绪"), true);
        emit analysisQueueDrained();
    }
}

void PikafishAnalyzer::beginBeforeAnalysis()
{
    latest_score_ = 0;
    latest_pv_.clear();
    const QString fen = toFen(current_.move.boardBefore, current_.move.side, current_.move.ply);
    sendCommand("position fen " + fen);
    sendCommand("go movetime 350");
    state_ = State::AnalyzingBefore;
    emit statusChanged(QString::fromUtf8(u8"正在复盘第 %1 步……").arg(current_.move.ply), true);
}

void PikafishAnalyzer::beginAfterAnalysis()
{
    latest_score_ = 0;
    latest_pv_.clear();
    const QString fen = toFen(current_.move.boardBefore, current_.move.side, current_.move.ply);
    sendCommand("position fen " + fen + " moves " + toUciMove(current_.move));
    sendCommand("go movetime 350");
    state_ = State::AnalyzingAfter;
}

void PikafishAnalyzer::finishCurrentAnalysis()
{
    AnalysisResult result;
    result.gameId = current_.gameId;
    result.ply = current_.move.ply;
    result.actualMove = toUciMove(current_.move);
    result.bestMove = best_move_;
    result.actualNotation = toChineseNotation(current_.move.boardBefore,
                                              current_.move.side,
                                              result.actualMove);
    result.bestNotation = toChineseNotation(current_.move.boardBefore,
                                            current_.move.side,
                                            result.bestMove);
    result.bestScore = best_score_;
    result.actualScore = -latest_score_;
    result.scoreLoss = calculateScoreLoss(result);
    result.category = categoryForLoss(result.scoreLoss);
    result.rawPrincipalVariation = best_pv_;
    result.rawActualPrincipalVariation = latest_pv_;
    result.sideToMove = current_.move.side;
    result.principalVariation = toChinesePrincipalVariation(
        current_.move.boardBefore, current_.move.side, best_pv_);
    result.actualPrincipalVariation = toChinesePrincipalVariation(
        current_.move.boardAfter,
        current_.move.side == XiangqiGame::Side::Red
            ? XiangqiGame::Side::Black : XiangqiGame::Side::Red,
        latest_pv_);
    result.explanation = explanationFor(result);
    result.boardBefore = QString::fromStdString(current_.move.boardBefore);
    result.thinkingTimeMs = current_.move.thinkingTimeMs;
    emit analysisReady(result);

    state_ = State::Idle;
    processNextRequest();
    if (state_ == State::Idle) {
        emit statusChanged(QString::fromUtf8(u8"皮卡鱼已就绪"), true);
        emit analysisQueueDrained();
    }
}

void PikafishAnalyzer::sendCommand(const QString &command)
{
    if (process_.state() == QProcess::Running) {
        process_.write(command.toUtf8() + '\n');
    }
}

QString PikafishAnalyzer::findEngineExecutable()
{
    const QString configured = QProcessEnvironment::systemEnvironment().value("PIKAFISH_PATH");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) {
        return QFileInfo(configured).absoluteFilePath();
    }

    const QString root = findProjectRoot();
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(root).filePath("engines/pikafish/pikafish.exe"),
        QDir(root).filePath("third_party/pikafish/pikafish.exe"),
        QDir(applicationDirectory).filePath("engines/pikafish/pikafish.exe"),
        QDir(applicationDirectory).filePath("engines/pikafish.exe")
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return QString();
}

QString PikafishAnalyzer::findProjectRoot()
{
    const QStringList startingPoints = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath()
    };
    for (const QString &start : startingPoints) {
        QDir directory(start);
        for (int level = 0; level < 7; ++level) {
            if (directory.exists("engine_py") && directory.exists("CMakeLists.txt")) {
                return directory.absolutePath();
            }
            if (!directory.cdUp()) {
                break;
            }
        }
    }
    return QDir::currentPath();
}

QString PikafishAnalyzer::toFen(const std::string &board, XiangqiGame::Side side, int ply)
{
    QString placement;
    int empty = 0;
    for (char raw : board) {
        if (raw == '/') {
            if (empty > 0) {
                placement += QString::number(empty);
                empty = 0;
            }
            placement += '/';
            continue;
        }
        if (raw == '.') {
            ++empty;
            continue;
        }
        if (empty > 0) {
            placement += QString::number(empty);
            empty = 0;
        }
        char code = raw;
        switch (raw) {
        case 'H': code = 'N'; break;
        case 'h': code = 'n'; break;
        case 'E': code = 'B'; break;
        case 'e': code = 'b'; break;
        case 'S': code = 'P'; break;
        case 's': code = 'p'; break;
        default: break;
        }
        placement += QChar(code);
    }
    if (empty > 0) {
        placement += QString::number(empty);
    }

    const QString sideCode = side == XiangqiGame::Side::Red ? "w" : "b";
    const int fullMove = std::max(1, (ply + 1) / 2);
    return QString("%1 %2 - - 0 %3").arg(placement, sideCode).arg(fullMove);
}

QString PikafishAnalyzer::toUciMove(const XiangqiGame::MoveRecord &move)
{
    auto square = [](int row, int col) {
        return QString(QChar('a' + col)) + QChar('9' - row);
    };
    return square(move.fromRow, move.fromCol) + square(move.toRow, move.toCol);
}

QString PikafishAnalyzer::toChineseNotation(const std::string &board,
                                            XiangqiGame::Side side,
                                            const QString &uciMove)
{
    if (uciMove.size() < 4) {
        return uciMove;
    }
    const int fromCol = uciMove[0].unicode() - QChar('a').unicode();
    const int fromRow = QChar('9').unicode() - uciMove[1].unicode();
    const int toCol = uciMove[2].unicode() - QChar('a').unicode();
    const int toRow = QChar('9').unicode() - uciMove[3].unicode();
    if (!XiangqiGame::inBounds(fromRow, fromCol) ||
        !XiangqiGame::inBounds(toRow, toCol)) {
        return uciMove;
    }

    const int index = fromRow * 10 + fromCol;
    if (index < 0 || index >= static_cast<int>(board.size())) {
        return uciMove;
    }
    const char code = static_cast<char>(std::toupper(
        static_cast<unsigned char>(board[static_cast<std::size_t>(index)])));
    QString piece;
    switch (code) {
    case 'K': piece = side == XiangqiGame::Side::Red
                          ? QString::fromUtf8(u8"帅") : QString::fromUtf8(u8"将"); break;
    case 'A': piece = side == XiangqiGame::Side::Red
                          ? QString::fromUtf8(u8"仕") : QString::fromUtf8(u8"士"); break;
    case 'E': piece = side == XiangqiGame::Side::Red
                          ? QString::fromUtf8(u8"相") : QString::fromUtf8(u8"象"); break;
    case 'H': piece = QString::fromUtf8(u8"马"); break;
    case 'R': piece = QString::fromUtf8(u8"车"); break;
    case 'C': piece = QString::fromUtf8(u8"炮"); break;
    case 'S': piece = side == XiangqiGame::Side::Red
                          ? QString::fromUtf8(u8"兵") : QString::fromUtf8(u8"卒"); break;
    default: return uciMove;
    }

    static const QStringList chineseNumbers = {
        QString(), QString::fromUtf8(u8"一"), QString::fromUtf8(u8"二"),
        QString::fromUtf8(u8"三"), QString::fromUtf8(u8"四"),
        QString::fromUtf8(u8"五"), QString::fromUtf8(u8"六"),
        QString::fromUtf8(u8"七"), QString::fromUtf8(u8"八"),
        QString::fromUtf8(u8"九")
    };
    auto fileNumber = [side](int col) {
        return side == XiangqiGame::Side::Red ? 9 - col : col + 1;
    };
    auto numberText = [&](int value) {
        return side == XiangqiGame::Side::Red
                   ? chineseNumbers.value(value)
                   : QString::number(value);
    };

    QString action;
    QString destination;
    if (fromRow == toRow) {
        action = QString::fromUtf8(u8"平");
        destination = numberText(fileNumber(toCol));
    } else {
        const bool forward = side == XiangqiGame::Side::Red
                                 ? toRow < fromRow : toRow > fromRow;
        action = forward ? QString::fromUtf8(u8"进") : QString::fromUtf8(u8"退");
        if (code == 'R' || code == 'C' || code == 'K' || code == 'S') {
            destination = numberText(std::abs(toRow - fromRow));
        } else {
            destination = numberText(fileNumber(toCol));
        }
    }
    return piece + numberText(fileNumber(fromCol)) + action + destination;
}

QString PikafishAnalyzer::toChinesePrincipalVariation(const std::string &board,
                                                       XiangqiGame::Side side,
                                                       const QString &uciMoves)
{
    XiangqiGame position;
    if (!position.loadPosition(board, side)) {
        return uciMoves;
    }

    QStringList notation;
    const QStringList moves = uciMoves.split(' ', Qt::SkipEmptyParts);
    for (const QString &move : moves) {
        if (move.size() < 4) {
            continue;
        }
        const XiangqiGame::Side movingSide = position.sideToMove();
        notation.push_back(toChineseNotation(position.boardString(), movingSide, move));

        const int fromCol = move[0].unicode() - QChar('a').unicode();
        const int fromRow = QChar('9').unicode() - move[1].unicode();
        const int toCol = move[2].unicode() - QChar('a').unicode();
        const int toRow = QChar('9').unicode() - move[3].unicode();
        if (!XiangqiGame::inBounds(fromRow, fromCol) ||
            !XiangqiGame::inBounds(toRow, toCol) ||
            !position.move(fromRow, fromCol, toRow, toCol)) {
            notation.back() += QString::fromUtf8(u8"（") + move + QString::fromUtf8(u8"）");
            break;
        }
    }
    return notation.isEmpty() ? uciMoves
                              : notation.join(QString::fromUtf8(u8" → "));
}

QString PikafishAnalyzer::categoryForLoss(int loss)
{
    if (loss <= 30) return "excellent";
    if (loss <= 80) return "inaccuracy";
    if (loss <= 200) return "mistake";
    return "blunder";
}

QString PikafishAnalyzer::explanationFor(const AnalysisResult &result)
{
    if (result.actualMove == result.bestMove || result.scoreLoss <= 30) {
        return QString::fromUtf8(u8"这一步与引擎首选接近，说明你的局面判断基本准确。");
    }
    if (result.bestScore > 90000 && result.actualScore > 500) {
        return QString::fromUtf8(
            u8"这一步错过了引擎发现的强制将杀，但仍然保持明显优势，并不是局势崩溃。"
            u8"建议把该局面作为“寻找最快将杀”的专项题。");
    }
    if (result.scoreLoss <= 80) {
        return QString::fromUtf8(u8"这是一次轻微失误。建议比较推荐变化，观察是否有更积极的出子或子力协调方式。");
    }
    if (result.scoreLoss <= 200) {
        return QString::fromUtf8(u8"这是一次明显失误。落子前应依次检查：对方将军、吃子和直接威胁。");
    }
    return QString::fromUtf8(u8"这是一次严重失误。建议把这个局面加入专项训练，并重新计算双方至少三步的强制变化。");
}

int PikafishAnalyzer::calculateScoreLoss(const AnalysisResult &result)
{
    if (result.actualMove == result.bestMove) {
        return 0;
    }

    constexpr int mateThreshold = 90000;
    if (result.bestScore > mateThreshold) {
        if (result.actualScore > mateThreshold) {
            return std::min(30, std::max(0, result.bestScore - result.actualScore));
        }
        if (result.actualScore > 500) {
            return 100;
        }
        if (result.actualScore > 0) {
            return 180;
        }
        return 300;
    }
    return std::max(0, result.bestScore - result.actualScore);
}

bool PikafishAnalyzer::parseScoreAndPv(const QString &line, int *score, QString *pv)
{
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    const int scoreIndex = parts.indexOf("score");
    if (scoreIndex < 0 || scoreIndex + 2 >= parts.size()) {
        return false;
    }

    bool ok = false;
    if (parts[scoreIndex + 1] == "cp") {
        const int parsed = parts[scoreIndex + 2].toInt(&ok);
        if (ok) {
            *score = parsed;
        }
    } else if (parts[scoreIndex + 1] == "mate") {
        const int movesToMate = parts[scoreIndex + 2].toInt(&ok);
        if (ok) {
            // UCI may report "mate 0" when the side to move has no legal
            // reply. Zero therefore belongs to the losing side.
            *score = movesToMate > 0 ? 100000 - movesToMate : -100000 - movesToMate;
        }
    }
    if (!ok) {
        return false;
    }

    const int pvIndex = parts.indexOf("pv");
    if (pvIndex >= 0 && pvIndex + 1 < parts.size()) {
        *pv = parts.mid(pvIndex + 1).join(' ');
    }
    return true;
}
