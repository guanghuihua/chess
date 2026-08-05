#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "xiangqi_board_widget.h"

#include <QFont>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto *central = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    board_widget_ = new XiangqiBoardWidget(central);
    mainLayout->addWidget(board_widget_, 3);

    auto *panel = new QWidget(central);
    panel->setMinimumWidth(360);
    panel->setMaximumWidth(460);
    auto *panelLayout = new QVBoxLayout(panel);

    auto *title = new QLabel(QString::fromUtf8(u8"中国象棋针对性训练系统"), panel);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    panelLayout->addWidget(title);

    engine_status_label_ = new QLabel(QString::fromUtf8(u8"正在启动皮卡鱼……"), panel);
    engine_status_label_->setWordWrap(true);
    panelLayout->addWidget(engine_status_label_);

    coach_status_label_ = new QLabel(QString::fromUtf8(u8"正在检查 DeepSeek 配置……"), panel);
    coach_status_label_->setWordWrap(true);
    panelLayout->addWidget(coach_status_label_);

    panelLayout->addWidget(new QLabel(QString::fromUtf8(u8"本局实时复盘"), panel));
    analysis_browser_ = new QTextBrowser(panel);
    analysis_browser_->setPlaceholderText(QString::fromUtf8(
        u8"你执红方。每走一步，系统会显示推荐走法、局面损失和训练建议。"));
    panelLayout->addWidget(analysis_browser_, 3);

    panelLayout->addWidget(new QLabel(QString::fromUtf8(u8"个人训练统计"), panel));
    stats_browser_ = new QTextBrowser(panel);
    stats_browser_->setMaximumHeight(160);
    panelLayout->addWidget(stats_browser_);

    database_label_ = new QLabel(panel);
    database_label_->setWordWrap(true);
    database_label_->setStyleSheet("color: #666; font-size: 11px;");
    panelLayout->addWidget(database_label_);

    auto *newGameButton = new QPushButton(QString::fromUtf8(u8"开始新对局"), panel);
    auto *deepSeekButton = new QPushButton(QString::fromUtf8(u8"DeepSeek 设置"), panel);
    panelLayout->addWidget(deepSeekButton);
    panelLayout->addWidget(newGameButton);

    mainLayout->addWidget(panel, 2);
    setCentralWidget(central);
    setWindowTitle(QString::fromUtf8(u8"象棋个性化训练系统"));
    resize(1180, 780);

    analyzer_ = new PikafishAnalyzer(this);
    coach_ = new DeepSeekCoach(this);
    connect(board_widget_, &XiangqiBoardWidget::moveCompleted,
            this, &MainWindow::handleMoveCompleted);
    connect(board_widget_, &XiangqiBoardWidget::gameEnded,
            this, &MainWindow::handleGameEnded);
    connect(analyzer_, &PikafishAnalyzer::analysisReady,
            this, &MainWindow::handleAnalysis);
    connect(analyzer_, &PikafishAnalyzer::statusChanged,
            this, [this](const QString &message, bool available) {
                engine_status_label_->setText(message);
                engine_status_label_->setStyleSheet(available ? "color: #267326;" : "color: #a33;");
            });
    connect(coach_, &DeepSeekCoach::coachingReady,
            this, &MainWindow::handleCoaching);
    connect(coach_, &DeepSeekCoach::statusChanged,
            this, [this](const QString &message, bool available) {
                coach_status_label_->setText(message);
                coach_status_label_->setStyleSheet(available ? "color: #267326;" : "color: #8a5a00;");
            });
    connect(newGameButton, &QPushButton::clicked,
            this, &MainWindow::startNewGame);
    connect(deepSeekButton, &QPushButton::clicked,
            this, &MainWindow::configureDeepSeek);

    initializeTrainingSystem();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeTrainingSystem()
{
    QString error;
    if (!database_.open(&error)) {
        QMessageBox::critical(this, QString::fromUtf8(u8"数据库错误"), error);
        database_label_->setText(QString::fromUtf8(u8"数据库不可用"));
        return;
    }
    database_label_->setText(QString::fromUtf8(u8"数据保存在：") + database_.databasePath());
    current_game_id_ = database_.startGame(&error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建对局"), error);
    }
    refreshStats();
}

void MainWindow::handleMoveCompleted()
{
    const auto &history = board_widget_->game().moveHistory();
    if (history.empty()) {
        return;
    }
    const auto &move = history.back();
    QString error;
    if (current_game_id_ >= 0 && !database_.recordMove(current_game_id_, move, &error)) {
        engine_status_label_->setText(QString::fromUtf8(u8"保存走法失败：") + error);
    }

    if (move.side == XiangqiGame::Side::Red && current_game_id_ >= 0) {
        analyzer_->analyzeMove(current_game_id_, move);
    }
}

void MainWindow::handleGameEnded()
{
    if (current_game_id_ < 0) {
        return;
    }
    QString error;
    if (!database_.finishGame(current_game_id_, board_widget_->game().result(), &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"保存结果失败"), error);
    }
    refreshStats();
}

void MainWindow::handleAnalysis(const PikafishAnalyzer::AnalysisResult &result)
{
    QString error;
    if (!database_.recordAnalysis(result.gameId, result.ply, result.actualMove,
                                  result.bestMove, result.bestScore, result.actualScore,
                                  result.scoreLoss, result.category,
                                  result.principalVariation, &error)) {
        engine_status_label_->setText(QString::fromUtf8(u8"保存分析失败：") + error);
    }

    QString categoryText;
    QString color;
    if (result.category == "excellent") {
        categoryText = QString::fromUtf8(u8"优秀");
        color = "#237a3b";
    } else if (result.category == "inaccuracy") {
        categoryText = QString::fromUtf8(u8"轻微失误");
        color = "#9a7b00";
    } else if (result.category == "mistake") {
        categoryText = QString::fromUtf8(u8"明显失误");
        color = "#d2691e";
    } else {
        categoryText = QString::fromUtf8(u8"严重失误");
        color = "#b22222";
    }

    const QString html = QString(
        "<div style='margin-bottom:12px'>"
        "<b>第 %1 步：</b><span style='color:%2'><b>%3</b></span><br>"
        "实际走法：<b>%4</b>（<code>%5</code>）<br>"
        "推荐走法：<b>%6</b>（<code>%7</code>）<br>"
        "评分：%8 → %9　局面损失：%10<br>%11<br>"
        "<small>推荐变化：%12</small></div>")
        .arg(result.ply)
        .arg(color, categoryText)
        .arg(result.actualNotation.toHtmlEscaped(), result.actualMove.toHtmlEscaped())
        .arg(result.bestNotation.toHtmlEscaped(), result.bestMove.toHtmlEscaped())
        .arg(result.bestScore)
        .arg(result.actualScore)
        .arg(result.scoreLoss)
        .arg(result.explanation.toHtmlEscaped(),
             result.principalVariation.toHtmlEscaped());
    analysis_browser_->append(html);
    refreshStats();
    if (result.scoreLoss > 30) {
        coach_->requestCoaching(result, database_.trainingStats());
    }
}

void MainWindow::handleCoaching(const DeepSeekCoach::CoachingResult &result)
{
    QString error;
    if (!database_.recordCoaching(result.gameId, result.ply, result.model,
                                  result.diagnosis, result.evidence,
                                  result.trainingTask, result.reflectionQuestion,
                                  &error)) {
        coach_status_label_->setText(QString::fromUtf8(u8"保存 AI 建议失败：") + error);
    }

    const QString html = QString(
        "<div style='border-left:4px solid #5b4bb7; padding-left:10px; margin:8px 0 16px 0'>"
        "<b>AI 教练 · 第 %1 步</b><br>"
        "<b>诊断：</b>%2<br>"
        "<b>依据：</b>%3<br>"
        "<b>针对训练：</b>%4<br>"
        "<b>复盘问题：</b>%5"
        "</div>")
        .arg(result.ply)
        .arg(result.diagnosis.toHtmlEscaped(),
             result.evidence.toHtmlEscaped(),
             result.trainingTask.toHtmlEscaped(),
             result.reflectionQuestion.toHtmlEscaped());
    analysis_browser_->append(html);
    refreshStats();
}

void MainWindow::startNewGame()
{
    if (!board_widget_->game().moveHistory().empty() &&
        board_widget_->game().result() == XiangqiGame::GameResult::Ongoing) {
        const auto answer = QMessageBox::question(
            this, QString::fromUtf8(u8"开始新对局"),
            QString::fromUtf8(u8"当前对局尚未结束，确定开始新对局吗？"));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    board_widget_->newGame();
    analysis_browser_->clear();
    QString error;
    current_game_id_ = database_.startGame(&error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建新对局"), error);
    }
    refreshStats();
}

void MainWindow::configureDeepSeek()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8(u8"DeepSeek API 设置"));
    dialog.setMinimumWidth(500);

    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(QString::fromUtf8(
        u8"密钥将保存在 Windows Credential Manager 中，不会写入源码、SQLite 或普通配置文件。"),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *form = new QFormLayout;
    auto *apiKeyEdit = new QLineEdit(&dialog);
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    apiKeyEdit->setPlaceholderText(coach_->isConfigured()
        ? QString::fromUtf8(u8"已经保存密钥；输入新密钥可替换")
        : QString::fromUtf8(u8"sk-……"));
    form->addRow(QString::fromUtf8(u8"API Key："), apiKeyEdit);
    layout->addLayout(form);

    auto *resultLabel = new QLabel(&dialog);
    resultLabel->setWordWrap(true);
    layout->addWidget(resultLabel);

    auto *buttonRow = new QHBoxLayout;
    auto *saveButton = new QPushButton(QString::fromUtf8(u8"保存并测试连接"), &dialog);
    auto *deleteButton = new QPushButton(QString::fromUtf8(u8"删除已保存密钥"), &dialog);
    auto *closeButton = new QPushButton(QString::fromUtf8(u8"关闭"), &dialog);
    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(saveButton, &QPushButton::clicked, &dialog, [this, apiKeyEdit, resultLabel] {
        QString error;
        if (!coach_->saveApiKey(apiKeyEdit->text(), &error)) {
            resultLabel->setStyleSheet("color: #a33;");
            resultLabel->setText(QString::fromUtf8(u8"保存失败：") + error);
            return;
        }
        apiKeyEdit->clear();
        resultLabel->setStyleSheet("color: #555;");
        resultLabel->setText(QString::fromUtf8(u8"已安全保存，正在连接 DeepSeek……"));
        coach_->testConnection();
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [this, resultLabel] {
        if (QMessageBox::question(this, QString::fromUtf8(u8"删除密钥"),
                                  QString::fromUtf8(u8"确定删除本机保存的 DeepSeek 密钥吗？"))
            != QMessageBox::Yes) {
            return;
        }
        QString error;
        const bool removed = coach_->removeApiKey(&error);
        resultLabel->setStyleSheet(removed ? "color: #267326;" : "color: #a33;");
        resultLabel->setText(removed ? QString::fromUtf8(u8"密钥已删除")
                                     : QString::fromUtf8(u8"删除失败：") + error);
    });
    connect(coach_, &DeepSeekCoach::connectionTested, &dialog,
            [resultLabel](bool success, const QString &message) {
                resultLabel->setStyleSheet(success ? "color: #267326;" : "color: #a33;");
                resultLabel->setText(message);
            });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::refreshStats()
{
    const GameDatabase::TrainingStats stats = database_.trainingStats();
    stats_browser_->setHtml(QString::fromUtf8(
        u8"累计对局：%1<br>已分析红方走法：%2　AI讲解：%3<br>"
        u8"优秀：%4　轻微失误：%5<br>"
        u8"明显失误：%6　严重失误：%7<br>"
        u8"平均局面损失：%8")
        .arg(stats.games)
        .arg(stats.analyzedMoves)
        .arg(stats.coachedMoves)
        .arg(stats.excellentMoves)
        .arg(stats.inaccuracies)
        .arg(stats.mistakes)
        .arg(stats.blunders)
        .arg(stats.averageLoss, 0, 'f', 1));
}
