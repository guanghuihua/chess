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
    void generatedExerciseRequested(const QString &requestId);
    void wrongLineRequested(const QString &requestId, const QString &boardBefore,
                            const QString &boardAfterWrong, const QString &wrongMove);

public slots:
    void receiveCoachReply(const QString &requestId, const QString &answer,
                           const QString &errorMessage);
    void generatedExerciseReady(const QString &requestId);
    void generatedExerciseFailed(const QString &requestId, const QString &errorMessage);
    void wrongLineAnalysisFinished(const QString &requestId);
    void wrongLineAnalysisFailed(const QString &requestId, const QString &errorMessage);

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
    QPushButton *generate_button_ = nullptr;
    QPushButton *undo_button_ = nullptr;
    QPushButton *review_wrong_button_ = nullptr;
    QPushButton *library_button_ = nullptr;
    QVector<GameDatabase::TrainingPosition> positions_;
    int current_index_ = -1;
    int hint_count_ = 0;
    QElapsedTimer timer_;
    QString coach_request_id_;
    QString generation_request_id_;
    QString pending_move_;
    qint64 pending_move_thinking_time_ms_ = 0;
    QString last_submitted_move_;
    bool last_submission_was_error_ = false;
    QString wrong_line_request_id_;
    QString background_generation_request_id_;

    void loadSession();
    void loadCurrentPosition();
    void handleMove(const QString &uciMove);
    void confirmCurrentMove();
    void undoCurrentMove();
    void showHint();
    void nextPosition();
    void requestEndgameCoaching();
    void requestGeneratedExercise();
    void requestBackgroundExercise();
    void requestWrongLineReview();
    void openTrainingLibrary();
    void requestAutomaticCoaching(const QString &uciMove, bool correct);
    static QString displayMove(const QString &board, const QString &uciMove);
    static QString displayVariation(const QString &board, const QString &variation);
};

#endif // TRAINING_DIALOG_H
