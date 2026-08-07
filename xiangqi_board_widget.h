#ifndef XIANGQI_BOARD_WIDGET_H
#define XIANGQI_BOARD_WIDGET_H

#include <optional>

#include <QElapsedTimer>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "xiangqi_game.h"

class QPaintEvent;
class QMouseEvent;
class QEvent;
class QPainter;
class QPoint;
class QPointF;
class QRectF;

class XiangqiBoardWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Difficulty
    {
        Beginner,
        Elementary,
        Intermediate,
        Advanced
    };

    explicit XiangqiBoardWidget(QWidget *parent = nullptr,
                                bool opponentEnabled = true);
    ~XiangqiBoardWidget() override;

    const XiangqiGame &game() const;
    void newGame();
    bool resign(XiangqiGame::Side side);
    int undoTurn();
    bool undoTrainingMove();
    bool loadTrainingPosition(const std::string &board);
    bool loadReviewPosition(const std::string &board, XiangqiGame::Side sideToMove,
                            int fromRow = -1, int fromCol = -1,
                            int toRow = -1, int toCol = -1);
    bool loadBranchPosition(const std::string &board, XiangqiGame::Side sideToMove);
    void setDifficulty(Difficulty difficulty);
    Difficulty difficulty() const;

signals:
    void moveCompleted();
    void gameEnded();
    void trainingMoveMade(const QString &uciMove);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Cell
    {
        int row;
        int col;
    };

    XiangqiGame game_;
    std::optional<Cell> selected_;
    std::optional<Cell> hovered_;
    QProcess engine_process_;
    QTimer engine_timeout_;
    QString engine_buffer_;
    QElapsedTimer turn_timer_;
    bool engine_busy_ = false;
    bool engine_ready_ = false;
    bool ignore_next_bestmove_ = false;
    bool shutting_down_ = false;
    bool opponent_enabled_ = true;
    bool training_mode_ = false;
    bool training_locked_ = false;
    std::optional<std::array<int, 4>> review_move_;
    Difficulty difficulty_ = Difficulty::Elementary;

    QRectF boardRect() const;
    double cellSize() const;
    QPointF intersectionPoint(int row, int col) const;
    std::optional<Cell> hitTest(const QPoint &position) const;

    void drawBoard(QPainter &painter);
    void drawLastMove(QPainter &painter);
    void drawPieces(QPainter &painter);
    void drawSelection(QPainter &painter);
    void drawMoveHints(QPainter &painter);

    void startEngine();
    void requestEngineMove();
    void handleEngineOutput();
    void handleEngineError();
    void showGameResult();
    QString findEngineRoot() const;
    QString findPikafishExecutable() const;
    QString currentPositionFen() const;
    QString searchCommand() const;
};

#endif // XIANGQI_BOARD_WIDGET_H
