#ifndef GAME_REVIEW_DIALOG_H
#define GAME_REVIEW_DIALOG_H

#include <QDialog>
#include <QVector>

#include "game_database.h"

class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QTabWidget;
class QTextBrowser;
class XiangqiBoardWidget;

class GameReviewDialog : public QDialog
{
public:
    explicit GameReviewDialog(GameDatabase *database, qint64 userId,
                              QWidget *parent = nullptr);

private:
    GameDatabase *database_ = nullptr;
    qint64 user_id_ = -1;
    qint64 game_id_ = -1;
    QVector<GameDatabase::RecordedMove> moves_;
    int position_index_ = 0;

    QListWidget *game_list_ = nullptr;
    XiangqiBoardWidget *board_ = nullptr;
    QLabel *game_title_ = nullptr;
    QLabel *position_label_ = nullptr;
    QSlider *timeline_ = nullptr;
    QPushButton *first_button_ = nullptr;
    QPushButton *previous_button_ = nullptr;
    QPushButton *next_button_ = nullptr;
    QPushButton *last_button_ = nullptr;
    QTabWidget *detail_tabs_ = nullptr;
    QTextBrowser *move_detail_ = nullptr;
    QTextBrowser *whole_review_ = nullptr;

    void loadGames();
    void loadSelectedGame();
    void showPosition(int index);
    void updateNavigation();
    QString moveHtml(const GameDatabase::RecordedMove &move) const;
    QString reviewHtml(qint64 gameId) const;
};

#endif // GAME_REVIEW_DIALOG_H
