#include "xiangqi_board_widget.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
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

XiangqiBoardWidget::XiangqiBoardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(620, 700);
    turn_timer_.start();

    engine_timeout_.setSingleShot(true);
    engine_timeout_.setInterval(engineTimeoutMilliseconds);

    connect(&engine_process_, &QProcess::readyReadStandardOutput,
            this, [this] { handleEngineOutput(); });
    connect(&engine_process_, &QProcess::readyReadStandardError,
            this, [this] { handleEngineError(); });
    connect(&engine_process_, &QProcess::started,
            this, [this] { requestEngineMove(); });
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
                             QString::fromUtf8(u8"Python \u5f15\u64ce\u672a\u5728 15 \u79d2\u5185\u8fd4\u56de\u8d70\u6cd5\u3002"));
    });

    startEngine();
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
    engine_buffer_.clear();
    selected_.reset();
    if (engine_process_.state() != QProcess::NotRunning) {
        engine_process_.kill();
        engine_process_.waitForFinished(1000);
    }
    game_ = XiangqiGame();
    turn_timer_.restart();
    startEngine();
    update();
}

void XiangqiBoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawBoard(painter);
    drawPieces(painter);
    drawSelection(painter);
}

void XiangqiBoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || engine_busy_ ||
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

    const QString root = findEngineRoot();
    if (root.isEmpty()) {
        return;
    }

    QString python = "python";
    const QString condaPython = "E:/Anaconda/envs/chess/python.exe";
    if (QFileInfo::exists(condaPython)) {
        python = condaPython;
    }

    engine_process_.setWorkingDirectory(root);
    engine_process_.setProgram(python);
    engine_process_.setArguments({"-u", "-m", "engine_py.engine", "--protocol", "--depth", "2"});
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

    const QString command = QString("position: %1 side:black\n")
                                .arg(QString::fromStdString(game_.boardString()));
    if (engine_process_.write(command.toUtf8()) >= 0) {
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

        if (line.startsWith("error:")) {
            engine_busy_ = false;
            engine_timeout_.stop();
            QMessageBox::warning(this,
                                 QString::fromUtf8(u8"\u5f15\u64ce\u9519\u8bef"),
                                 line);
            continue;
        }
        if (!line.startsWith("move:")) {
            continue;
        }

        engine_busy_ = false;
        engine_timeout_.stop();
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() != 5) {
            continue;
        }

        const int fromRow = parts[1].toInt();
        const int fromCol = parts[2].toInt();
        const int toRow = parts[3].toInt();
        const int toCol = parts[4].toInt();
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
                             QString::fromUtf8(u8"Python \u5f15\u64ce\u9519\u8bef"),
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
            if (directory.exists("engine_py")) {
                return directory.absolutePath();
            }
            if (!directory.cdUp()) {
                break;
            }
        }
    }
    return QString();
}
