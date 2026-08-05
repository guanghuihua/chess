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
class QPainter;
class QPoint;
class QPointF;
class QRectF;

class XiangqiBoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit XiangqiBoardWidget(QWidget *parent = nullptr);
    ~XiangqiBoardWidget() override;

    const XiangqiGame &game() const;
    void newGame();

signals:
    void moveCompleted();
    void gameEnded();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct Cell
    {
        int row;
        int col;
    };

    XiangqiGame game_;
    std::optional<Cell> selected_;
    QProcess engine_process_;
    QTimer engine_timeout_;
    QString engine_buffer_;
    QElapsedTimer turn_timer_;
    bool engine_busy_ = false;
    bool shutting_down_ = false;

    QRectF boardRect() const;
    double cellSize() const;
    QPointF intersectionPoint(int row, int col) const;
    std::optional<Cell> hitTest(const QPoint &position) const;

    void drawBoard(QPainter &painter);
    void drawPieces(QPainter &painter);
    void drawSelection(QPainter &painter);

    void startEngine();
    void requestEngineMove();
    void handleEngineOutput();
    void handleEngineError();
    void showGameResult();
    QString findEngineRoot() const;
};

#endif // XIANGQI_BOARD_WIDGET_H
