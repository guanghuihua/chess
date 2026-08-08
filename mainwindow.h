#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include "game_database.h"
#include "deepseek_coach.h"
#include "pikafish_analyzer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class QComboBox;
class QTabWidget;
class QTextBrowser;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QFrame;
class QWidget;
class XiangqiBoardWidget;
class ProfileDashboardWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    XiangqiBoardWidget *board_widget_ = nullptr;
    QLabel *engine_status_label_ = nullptr;
    QLabel *coach_status_label_ = nullptr;
    QLabel *database_label_ = nullptr;
    QTextBrowser *analysis_browser_ = nullptr;
    QScrollArea *advice_scroll_ = nullptr;
    QWidget *advice_feed_ = nullptr;
    QVBoxLayout *advice_feed_layout_ = nullptr;
    QLineEdit *coach_question_edit_ = nullptr;
    QLineEdit *coach_thought_edit_ = nullptr;
    QPushButton *coach_question_button_ = nullptr;
    QTextBrowser *stats_browser_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QComboBox *user_combo_ = nullptr;
    ProfileDashboardWidget *profile_dashboard_ = nullptr;
    GameDatabase database_;
    PikafishAnalyzer *analyzer_ = nullptr;
    DeepSeekCoach *coach_ = nullptr;
    qint64 current_game_id_ = -1;
    qint64 active_user_id_ = 1;
    QSet<qint64> pending_game_reviews_;
    QString current_game_end_reason_ = QStringLiteral("normal");
    QString coach_chat_context_;
    QString coach_chat_history_;
    QString active_chat_request_id_;
    qint64 pending_chat_game_id_ = -1;
    int pending_chat_ply_ = 0;
    QHash<int, PikafishAnalyzer::AnalysisResult> current_analyses_;
    QHash<QString, DeepSeekCoach::ExerciseDraft> pending_exercise_drafts_;
    struct PendingTrainingLine
    {
        QString boardBefore;
        QString wrongMove;
    };
    QHash<QString, PendingTrainingLine> pending_training_lines_;

    void initializeTrainingSystem();
    void handleMoveCompleted();
    void handleGameEnded();
    void handleAnalysis(const PikafishAnalyzer::AnalysisResult &result);
    void handleCoaching(const DeepSeekCoach::CoachingResult &result);
    void handleGameReview(const DeepSeekCoach::GameReviewResult &result);
    void sendCoachQuestion();
    void handleChatReply(const QString &requestId, const QString &answer,
                         const QString &errorMessage);
    void showGameReviewPopup(const DeepSeekCoach::GameReviewResult &result,
                             const GameDatabase::GameReviewContext &context);
    void showEngineRecommendation(int ply);
    void appendCoachFeedbackControls(QFrame *card, qint64 gameId, int ply);
    QFrame *appendAdviceCard(const QString &title, const QString &lead,
                             const QStringList &sectionTitles = {},
                             const QStringList &sectionTexts = {},
                             const QString &tone = QStringLiteral("neutral"),
                             int ply = -1);
    void appendChatBubble(bool user, const QString &text, bool error = false);
    void clearAdviceCards();
    void markUndoneAdviceCards(int lastKeptPly);
    void scrollAdviceToBottom();
    void requestPendingGameReviews();
    void startNewGame();
    void resignGame();
    void undoTurn();
    void startPersonalTraining();
    void openGameReview();
    void createUser();
    void switchUser(int comboIndex);
    void populateUsers();
    void showMilestoneReportIfNeeded();
    void configureDeepSeek();
    void favoriteScore();
    void openFavoriteScores();
    void refreshStats();
    bool isCurrentMove(qint64 gameId, int ply, const QString &uciMove) const;
};

#endif // MAINWINDOW_H
