#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QObject>
#include <QSet>

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
    QTextBrowser *ai_advice_browser_ = nullptr;
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

    void initializeTrainingSystem();
    void handleMoveCompleted();
    void handleGameEnded();
    void handleAnalysis(const PikafishAnalyzer::AnalysisResult &result);
    void handleCoaching(const DeepSeekCoach::CoachingResult &result);
    void handleGameReview(const DeepSeekCoach::GameReviewResult &result);
    void requestPendingGameReviews();
    void startNewGame();
    void resignGame();
    void undoTurn();
    void startPersonalTraining();
    void createUser();
    void switchUser(int comboIndex);
    void populateUsers();
    void showMilestoneReportIfNeeded();
    void configureDeepSeek();
    void refreshStats();
    bool isCurrentMove(qint64 gameId, int ply, const QString &uciMove) const;
};

#endif // MAINWINDOW_H
