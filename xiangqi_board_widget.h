#ifndef XIANGQI_BOARD_WIDGET_H
#define XIANGQI_BOARD_WIDGET_H

class Xiangqi_board_widget : public QWidget
{
    Q_OBJECT
public:
    explicit Xiangqi_board_widget(QWidget *parent = nullptr);

protected:
    void painEvent(QPaintEvent *event) override;
signals:
};

#endif // XIANGQI_BOARD_WIDGET_H
