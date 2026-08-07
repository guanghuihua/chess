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
class QLineEdit;
class XiangqiBoardWidget;

class GameReviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GameReviewDialog(GameDatabase *database, qint64 userId,
                              QWidget *parent = nullptr);
    QVector<qint64> deletedGameIds() const;
    void receiveChatReply(const QString &requestId, const QString &answer,
                          const QString &errorMessage);

signals:
    void coachQuestionAsked(const QString &requestId,
                            const QString &evidenceContext,
                            const QString &conversationHistory,
                            const QString &question);

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
    QPushButton *branch_button_ = nullptr;
    QTabWidget *detail_tabs_ = nullptr;
    QTextBrowser *move_detail_ = nullptr;
    QTextBrowser *whole_review_ = nullptr;
    QTextBrowser *undo_review_ = nullptr;
    QTextBrowser *chat_browser_ = nullptr;
    QLineEdit *chat_edit_ = nullptr;
    QPushButton *chat_button_ = nullptr;
    QPushButton *delete_button_ = nullptr;
    QVector<qint64> deleted_game_ids_;
    QString chat_request_id_;
    qint64 chat_request_game_id_ = -1;
    int chat_request_ply_ = 0;
    QString chat_history_;

    void loadGames();
    void loadSelectedGame();
    void showPosition(int index);
    void updateNavigation();
    void deleteSelectedGame();
    void openBranch();
    void sendChatQuestion();
    void loadChatHistory();
    QString moveHtml(const GameDatabase::RecordedMove &move) const;
    QString reviewHtml(qint64 gameId) const;
    QString undoHtml(qint64 gameId) const;
    QString chatEvidenceContext() const;
};

#endif // GAME_REVIEW_DIALOG_H
