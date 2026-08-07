#ifndef TRAINING_DIALOG_H
#define TRAINING_DIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QVector>

#include "game_database.h"

class QLabel;
class QPushButton;
class QTextBrowser;
class XiangqiBoardWidget;

class TrainingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TrainingDialog(GameDatabase *database, qint64 userId,
                            QWidget *parent = nullptr);

signals:
    void coachQuestionAsked(const QString &requestId,
                            const QString &evidenceContext,
                            const QString &conversationHistory,
                            const QString &question);

public slots:
    void receiveCoachReply(const QString &requestId, const QString &answer,
                           const QString &errorMessage);

private:
    GameDatabase *database_ = nullptr;
    qint64 user_id_ = 1;
    XiangqiBoardWidget *board_ = nullptr;
    QLabel *progress_label_ = nullptr;
    QLabel *theme_label_ = nullptr;
    QLabel *source_label_ = nullptr;
    QTextBrowser *result_browser_ = nullptr;
    QPushButton *hint_button_ = nullptr;
    QPushButton *next_button_ = nullptr;
    QPushButton *ai_button_ = nullptr;
    QVector<GameDatabase::TrainingPosition> positions_;
    int current_index_ = -1;
    int hint_count_ = 0;
    QElapsedTimer timer_;
    QString coach_request_id_;

    void loadSession();
    void loadCurrentPosition();
    void handleMove(const QString &uciMove);
    void showHint();
    void nextPosition();
    void requestEndgameCoaching();
    static QString displayMove(const QString &uciMove);
};

#endif // TRAINING_DIALOG_H
