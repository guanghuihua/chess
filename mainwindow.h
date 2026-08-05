#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QObject>

#include "game_database.h"
#include "deepseek_coach.h"
#include "pikafish_analyzer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class QTextBrowser;
class XiangqiBoardWidget;

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
    QTextBrowser *stats_browser_ = nullptr;
    GameDatabase database_;
    PikafishAnalyzer *analyzer_ = nullptr;
    DeepSeekCoach *coach_ = nullptr;
    qint64 current_game_id_ = -1;

    void initializeTrainingSystem();
    void handleMoveCompleted();
    void handleGameEnded();
    void handleAnalysis(const PikafishAnalyzer::AnalysisResult &result);
    void handleCoaching(const DeepSeekCoach::CoachingResult &result);
    void startNewGame();
    void configureDeepSeek();
    void refreshStats();
};

#endif // MAINWINDOW_H
