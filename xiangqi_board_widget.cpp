#include "xiangqi_board_widget.h"

Xiangqi_board_widget::Xiangqi_board_widget(QWidget *parent)
    : QWidget{parent}
{}

void XiangqiBoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int margin = 30;
    const int w = width() - 2 * margin;
    const int h = height() - 2 * margin;
    const double cell = std::min(w / 8.0, h / 9.0);

    const double left = (width() - cell * 8) / 2.0;
    const double top = (height() - cell * 9) / 2.0;

    auto point = [&](int r, int c) {
        return QPointF(left + c * cell, top + r * cell);
    };

    p.setPen(QPen(QColor(120, 70, 30), 2));
    p.setBrush(QColor(245, 228, 196));

    // outer border
    p.drawRect(QRectF(left, top, cell * 8, cell * 9));

    // horizontal lines (10)
    for (int r = 0; r < 10; ++r) {
        p.drawLine(point(r, 0), point(r, 8));
    }

    // vertical lines (9) with river gap
    for (int c = 0; c < 9; ++c) {
        if (c == 0 || c == 8) {
            p.drawLine(point(0, c), point(9, c));
        } else {
            p.drawLine(point(0, c), point(4, c));
            p.drawLine(point(5, c), point(9, c));
        }
    }

    // palaces
    p.drawLine(point(0, 3), point(2, 5));
    p.drawLine(point(0, 5), point(2, 3));
    p.drawLine(point(7, 3), point(9, 5));
    p.drawLine(point(7, 5), point(9, 3));
}
