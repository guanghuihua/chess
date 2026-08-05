#include "xiangqi_board_widget.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLineF>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QProcessEnvironment>
#include <QRectF>
#include <QStringList>

namespace {
constexpr int margin = 32;
constexpr int engineTimeoutMilliseconds = 15000;

QString pieceLabel(const XiangqiGame::Piece &piece)
{
    using PieceType = XiangqiGame::PieceType;
    using Side = XiangqiGame::Side;

    switch (piece.type) {
    case PieceType::General:
        return QString::fromUtf8(piece.side == Side::Red ? u8"\u5e05" : u8"\u5c06");
    case PieceType::Advisor:
        return QString::fromUtf8(piece.side == Side::Red ? u8"\u4ed5" : u8"\u58eb");
    case PieceType::Elephant:
        return QString::fromUtf8(piece.side == Side::Red ? u8"\u76f8" : u8"\u8c61");
    case PieceType::Horse:
        return QString::fromUtf8(u8"\u9a6c");
    case PieceType::Rook:
        return QString::fromUtf8(u8"\u8f66");
    case PieceType::Cannon:
        return QString::fromUtf8(u8"\u70ae");
    case PieceType::Soldier:
        return QString::fromUtf8(piece.side == Side::Red ? u8"\u5175" : u8"\u5352");
    }
    return QString();
}
}

XiangqiBoardWidget::XiangqiBoardWidget(QWidget *parent, bool opponentEnabled)
    : QWidget(parent)
    , opponent_enabled_(opponentEnabled)
{
    setMinimumSize(540, 610);
    turn_timer_.start();

    engine_timeout_.setSingleShot(true);
    engine_timeout_.setInterval(engineTimeoutMilliseconds);

    connect(&engine_process_, &QProcess::readyReadStandardOutput,
            this, [this] { handleEngineOutput(); });
    connect(&engine_process_, &QProcess::readyReadStandardError,
            this, [this] { handleEngineError(); });
    connect(&engine_process_, &QProcess::started, this, [this] {
        engine_process_.write("uci\n");
    });
    connect(&engine_process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                engine_busy_ = false;
                engine_timeout_.stop();
                if (!shutting_down_ &&
                    game_.result() == XiangqiGame::GameResult::Ongoing &&
                    game_.sideToMove() == XiangqiGame::Side::Black) {
                    QTimer::singleShot(0, this, [this] { startEngine(); });
                }
            });
    connect(&engine_timeout_, &QTimer::timeout, this, [this] {
        engine_busy_ = false;
        engine_process_.kill();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u5f15\u64ce\u8d85\u65f6"),
                             QString::fromUtf8(u8"Pikafish \u672a\u5728 15 \u79d2\u5185\u8fd4\u56de\u8d70\u6cd5\u3002"));
    });

    if (opponent_enabled_) {
        startEngine();
    }
}

XiangqiBoardWidget::~XiangqiBoardWidget()
{
    shutting_down_ = true;
    if (engine_process_.state() != QProcess::NotRunning) {
        engine_process_.write("quit\n");
        engine_process_.waitForFinished(500);
        if (engine_process_.state() != QProcess::NotRunning) {
            engine_process_.kill();
            engine_process_.waitForFinished(500);
        }
    }
}

const XiangqiGame &XiangqiBoardWidget::game() const
{
    return game_;
}

void XiangqiBoardWidget::newGame()
{
    engine_timeout_.stop();
    engine_busy_ = false;
    engine_ready_ = false;
    engine_buffer_.clear();
    selected_.reset();
    if (engine_process_.state() != QProcess::NotRunning) {
        engine_process_.kill();
        engine_process_.waitForFinished(1000);
    }
    game_ = XiangqiGame();
    training_mode_ = false;
    training_locked_ = false;
    turn_timer_.restart();
    if (opponent_enabled_) {
        startEngine();
    }
    update();
}

bool XiangqiBoardWidget::loadTrainingPosition(const std::string &board)
{
    engine_timeout_.stop();
    engine_busy_ = false;
    selected_.reset();
    training_mode_ = true;
    training_locked_ = false;
    if (!game_.loadPosition(board, XiangqiGame::Side::Red)) {
        return false;
    }
    turn_timer_.restart();
    update();
    return true;
}

int XiangqiBoardWidget::undoTurn()
{
    if (game_.moveHistory().empty()) {
        return 0;
    }

    if (engine_busy_ && engine_process_.state() == QProcess::Running) {
        ignore_next_bestmove_ = true;
        engine_process_.write("stop\n");
    }
    engine_busy_ = false;
    engine_timeout_.stop();
    selected_.reset();

    int undone = 0;
    if (game_.undoLastMove()) {
        ++undone;
    }
    while (game_.sideToMove() != XiangqiGame::Side::Red &&
           !game_.moveHistory().empty() && undone < 2) {
        if (!game_.undoLastMove()) {
            break;
        }
        ++undone;
    }

    turn_timer_.restart();
    update();
    return undone;
}

void XiangqiBoardWidget::setDifficulty(Difficulty difficulty)
{
    difficulty_ = difficulty;
}

XiangqiBoardWidget::Difficulty XiangqiBoardWidget::difficulty() const
{
    return difficulty_;
}

void XiangqiBoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawBoard(painter);
    drawLastMove(painter);
    drawPieces(painter);
    drawSelection(painter);
}

void XiangqiBoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || engine_busy_ || training_locked_ ||
        game_.result() != XiangqiGame::GameResult::Ongoing ||
        game_.sideToMove() != XiangqiGame::Side::Red) {
        return;
    }

    const auto cell = hitTest(event->pos());
    if (!cell.has_value()) {
        selected_.reset();
        update();
        return;
    }

    if (selected_.has_value() &&
        game_.move(selected_->row, selected_->col, cell->row, cell->col,
                   turn_timer_.elapsed())) {
        selected_.reset();
        turn_timer_.restart();
        update();
        if (training_mode_) {
            const auto &move = game_.moveHistory().back();
            auto square = [](int row, int col) {
                return QString(QChar('a' + col)) + QChar('9' - row);
            };
            training_locked_ = true;
            emit trainingMoveMade(square(move.fromRow, move.fromCol) +
                                  square(move.toRow, move.toCol));
            return;
        }
        emit moveCompleted();
        const bool ended = game_.result() != XiangqiGame::GameResult::Ongoing;
        showGameResult();
        if (ended) {
            emit gameEnded();
        }
        requestEngineMove();
        return;
    }

    const auto &piece = game_.at(cell->row, cell->col);
    if (piece.has_value() && piece->side == XiangqiGame::Side::Red) {
        selected_ = cell;
    } else {
        selected_.reset();
    }
    update();
}

QRectF XiangqiBoardWidget::boardRect() const
{
    const double cell = cellSize();
    return QRectF((width() - cell * 8) / 2.0,
                  (height() - cell * 9) / 2.0,
                  cell * 8,
                  cell * 9);
}

double XiangqiBoardWidget::cellSize() const
{
    return std::max(1.0,
                    std::min((width() - 2.0 * margin) / 8.0,
                             (height() - 2.0 * margin) / 9.0));
}

QPointF XiangqiBoardWidget::intersectionPoint(int row, int col) const
{
    const QRectF rect = boardRect();
    const double cell = cellSize();
    return QPointF(rect.left() + col * cell, rect.top() + row * cell);
}

std::optional<XiangqiBoardWidget::Cell> XiangqiBoardWidget::hitTest(const QPoint &position) const
{
    const QRectF rect = boardRect();
    const double cell = cellSize();
    const int col = static_cast<int>(std::round((position.x() - rect.left()) / cell));
    const int row = static_cast<int>(std::round((position.y() - rect.top()) / cell));
    if (!XiangqiGame::inBounds(row, col)) {
        return std::nullopt;
    }

    const QPointF point = intersectionPoint(row, col);
    if (std::hypot(position.x() - point.x(), position.y() - point.y()) > cell * 0.45) {
        return std::nullopt;
    }
    return Cell{row, col};
}

void XiangqiBoardWidget::drawBoard(QPainter &painter)
{
    const QRectF rect = boardRect();

    painter.save();
    painter.setPen(QPen(QColor(120, 70, 30), 2));
    painter.setBrush(QColor(245, 228, 196));
    painter.drawRect(rect);

    for (int row = 0; row < 10; ++row) {
        painter.drawLine(intersectionPoint(row, 0), intersectionPoint(row, 8));
    }
    for (int col = 0; col < 9; ++col) {
        if (col == 0 || col == 8) {
            painter.drawLine(intersectionPoint(0, col), intersectionPoint(9, col));
        } else {
            painter.drawLine(intersectionPoint(0, col), intersectionPoint(4, col));
            painter.drawLine(intersectionPoint(5, col), intersectionPoint(9, col));
        }
    }

    painter.drawLine(intersectionPoint(0, 3), intersectionPoint(2, 5));
    painter.drawLine(intersectionPoint(0, 5), intersectionPoint(2, 3));
    painter.drawLine(intersectionPoint(7, 3), intersectionPoint(9, 5));
    painter.drawLine(intersectionPoint(7, 5), intersectionPoint(9, 3));

    QFont riverFont = painter.font();
    riverFont.setPointSizeF(cellSize() * 0.25);
    riverFont.setBold(true);
    painter.setFont(riverFont);
    painter.drawText(QRectF(rect.left(), intersectionPoint(4, 0).y(), rect.width(), cellSize()),
                     Qt::AlignCenter,
                     QString::fromUtf8(u8"\u695a \u6cb3        \u6c49 \u754c"));
    painter.restore();
}

void XiangqiBoardWidget::drawPieces(QPainter &painter)
{
    const double cell = cellSize();
    const double diameter = cell * 0.78;

    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 9; ++col) {
            const auto &piece = game_.at(row, col);
            if (!piece.has_value()) {
                continue;
            }

            const QPointF center = intersectionPoint(row, col);
            const QRectF pieceRect(center.x() - diameter / 2.0,
                                   center.y() - diameter / 2.0,
                                   diameter,
                                   diameter);
            const QColor color = piece->side == XiangqiGame::Side::Red
                                     ? QColor(205, 45, 45)
                                     : QColor(25, 25, 25);

            painter.save();
            painter.setPen(QPen(color, 2));
            painter.setBrush(QColor(252, 246, 232));
            painter.drawEllipse(pieceRect);

            QFont font = painter.font();
            font.setFamily("Microsoft YaHei");
            font.setBold(true);
            font.setPointSizeF(cell * 0.36);
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(pieceRect, Qt::AlignCenter, pieceLabel(*piece));
            painter.restore();
        }
    }
}

void XiangqiBoardWidget::drawLastMove(QPainter &painter)
{
    const auto &history = game_.moveHistory();
    if (history.empty()) {
        return;
    }

    const XiangqiGame::MoveRecord &move = history.back();
    const QPointF from = intersectionPoint(move.fromRow, move.fromCol);
    const QPointF to = intersectionPoint(move.toRow, move.toCol);
    const double cell = cellSize();
    const double markerRadius = cell * 0.43;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 196, 0, 65));
    painter.drawEllipse(from, markerRadius, markerRadius);
    painter.setBrush(QColor(40, 150, 75, 75));
    painter.drawEllipse(to, markerRadius, markerRadius);

    QLineF direction(from, to);
    if (direction.length() > 0.0) {
        const QPointF unit = (to - from) / direction.length();
        const QPointF normal(-unit.y(), unit.x());
        const QPointF lineStart = from + unit * (cell * 0.18);
        const QPointF lineEnd = to - unit * (cell * 0.22);

        painter.setPen(QPen(QColor(45, 125, 210, 180),
                            std::max(2.0, cell * 0.055),
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(lineStart, lineEnd);

        const double arrowLength = cell * 0.18;
        const double arrowWidth = cell * 0.11;
        const QPolygonF arrow{
            lineEnd,
            lineEnd - unit * arrowLength + normal * arrowWidth,
            lineEnd - unit * arrowLength - normal * arrowWidth
        };
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(45, 125, 210, 210));
        painter.drawPolygon(arrow);
    }

    painter.restore();
}

void XiangqiBoardWidget::drawSelection(QPainter &painter)
{
    if (!selected_.has_value()) {
        return;
    }

    const QPointF center = intersectionPoint(selected_->row, selected_->col);
    const double radius = cellSize() * 0.44;
    painter.save();
    painter.setPen(QPen(QColor(40, 160, 70), 4));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
    painter.restore();
}

void XiangqiBoardWidget::startEngine()
{
    if (engine_process_.state() != QProcess::NotRunning) {
        return;
    }

    const QString executable = findPikafishExecutable();
    if (executable.isEmpty()) {
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u5f15\u64ce\u9519\u8bef"),
                             QString::fromUtf8(u8"\u672a\u627e\u5230 Pikafish \u53ef\u6267\u884c\u6587\u4ef6\u3002"));
        return;
    }

    engine_ready_ = false;
    engine_process_.setWorkingDirectory(QFileInfo(executable).absolutePath());
    engine_process_.setProgram(executable);
    engine_process_.setArguments({});
    engine_process_.start();
}

void XiangqiBoardWidget::requestEngineMove()
{
    if (game_.result() != XiangqiGame::GameResult::Ongoing ||
        game_.sideToMove() != XiangqiGame::Side::Black || engine_busy_) {
        return;
    }

    if (engine_process_.state() != QProcess::Running) {
        startEngine();
        return;
    }
    if (!engine_ready_) {
        return;
    }

    engine_process_.write(("position fen " + currentPositionFen() + "\n").toUtf8());
    if (engine_process_.write((searchCommand() + "\n").toUtf8()) >= 0) {
        engine_busy_ = true;
        engine_timeout_.start();
    }
}

void XiangqiBoardWidget::handleEngineOutput()
{
    engine_buffer_ += QString::fromUtf8(engine_process_.readAllStandardOutput());
    int newline = -1;
    while ((newline = engine_buffer_.indexOf('\n')) >= 0) {
        const QString line = engine_buffer_.left(newline).trimmed();
        engine_buffer_.remove(0, newline + 1);

        if (line == "uciok") {
            engine_process_.write("setoption name Threads value 1\n");
            engine_process_.write("setoption name Hash value 64\n");
            engine_process_.write("setoption name MultiPV value 1\n");
            engine_process_.write("isready\n");
            continue;
        }
        if (line == "readyok") {
            engine_ready_ = true;
            requestEngineMove();
            continue;
        }
        if (!line.startsWith("bestmove ")) {
            continue;
        }

        if (ignore_next_bestmove_) {
            ignore_next_bestmove_ = false;
            engine_busy_ = false;
            engine_timeout_.stop();
            continue;
        }

        engine_busy_ = false;
        engine_timeout_.stop();
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2 || parts[1].size() < 4 || parts[1] == "(none)") {
            continue;
        }

        const QString move = parts[1];
        const int fromCol = move[0].unicode() - QChar('a').unicode();
        const int fromRow = QChar('9').unicode() - move[1].unicode();
        const int toCol = move[2].unicode() - QChar('a').unicode();
        const int toRow = QChar('9').unicode() - move[3].unicode();
        if (!game_.move(fromRow, fromCol, toRow, toCol, turn_timer_.elapsed())) {
            QMessageBox::warning(this,
                                 QString::fromUtf8(u8"\u5f15\u64ce\u8d70\u6cd5\u65e0\u6548"),
                                 line);
            continue;
        }

        turn_timer_.restart();
        update();
        emit moveCompleted();
        const bool ended = game_.result() != XiangqiGame::GameResult::Ongoing;
        showGameResult();
        if (ended) {
            emit gameEnded();
        }
    }
}

void XiangqiBoardWidget::handleEngineError()
{
    const QString error = QString::fromUtf8(engine_process_.readAllStandardError()).trimmed();
    if (!error.isEmpty()) {
        engine_busy_ = false;
        engine_timeout_.stop();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"Pikafish \u5f15\u64ce\u9519\u8bef"),
                             error);
    }
}

void XiangqiBoardWidget::showGameResult()
{
    QString message;
    switch (game_.result()) {
    case XiangqiGame::GameResult::RedWins:
        message = QString::fromUtf8(u8"\u7ea2\u65b9\u80dc\u5229");
        break;
    case XiangqiGame::GameResult::BlackWins:
        message = QString::fromUtf8(u8"\u9ed1\u65b9\u80dc\u5229");
        break;
    case XiangqiGame::GameResult::Draw:
        message = QString::fromUtf8(u8"\u548c\u68cb");
        break;
    case XiangqiGame::GameResult::Ongoing:
        return;
    }

    QMessageBox::information(this,
                             QString::fromUtf8(u8"\u6e38\u620f\u7ed3\u675f"),
                             message);
}

QString XiangqiBoardWidget::findEngineRoot() const
{
    const QStringList candidates = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath()
    };
    for (const QString &candidate : candidates) {
        QDir directory(candidate);
        for (int level = 0; level < 6; ++level) {
            if (directory.exists("CMakeLists.txt") &&
                (directory.exists("engines") || directory.exists("engine_py"))) {
                return directory.absolutePath();
            }
            if (!directory.cdUp()) {
                break;
            }
        }
    }
    return QString();
}

QString XiangqiBoardWidget::findPikafishExecutable() const
{
    const QString configured = QProcessEnvironment::systemEnvironment().value("PIKAFISH_PATH");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) {
        return QFileInfo(configured).absoluteFilePath();
    }

    const QString root = findEngineRoot();
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

QString XiangqiBoardWidget::currentPositionFen() const
{
    QString placement;
    int empty = 0;
    for (char raw : game_.boardString()) {
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
        switch (raw) {
        case 'H': raw = 'N'; break;
        case 'h': raw = 'n'; break;
        case 'E': raw = 'B'; break;
        case 'e': raw = 'b'; break;
        case 'S': raw = 'P'; break;
        case 's': raw = 'p'; break;
        default: break;
        }
        placement += QChar(raw);
    }
    if (empty > 0) {
        placement += QString::number(empty);
    }

    const QString side = game_.sideToMove() == XiangqiGame::Side::Red ? "w" : "b";
    const int fullMove = std::max(1, (static_cast<int>(game_.moveHistory().size()) + 2) / 2);
    return QString("%1 %2 - - 0 %3").arg(placement, side).arg(fullMove);
}

QString XiangqiBoardWidget::searchCommand() const
{
    switch (difficulty_) {
    case Difficulty::Beginner:
        return "go nodes 500";
    case Difficulty::Elementary:
        return "go nodes 2000";
    case Difficulty::Intermediate:
        return "go nodes 10000";
    case Difficulty::Advanced:
        return "go movetime 600";
    }
    return "go nodes 2000";
}
