#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "xiangqi_board_widget.h"
#include "training_dialog.h"
#include "profile_dashboard_widget.h"

#include <QFont>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto *central = new QWidget(this);
    central->setObjectName("appRoot");
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(16, 14, 16, 16);
    rootLayout->setSpacing(14);

    auto *header = new QFrame(central);
    header->setObjectName("headerCard");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 12, 16, 12);
    headerLayout->setSpacing(10);

    auto *brandLayout = new QVBoxLayout;
    brandLayout->setSpacing(1);
    auto *title = new QLabel(QString::fromUtf8(u8"中国象棋针对性训练系统"), header);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *subtitle = new QLabel(QString::fromUtf8(u8"对局 · 复盘 · 个性化训练"), header);
    subtitle->setObjectName("subtitle");
    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);
    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();

    headerLayout->addWidget(new QLabel(QString::fromUtf8(u8"用户"), header));
    user_combo_ = new QComboBox(header);
    user_combo_->setMinimumWidth(110);
    headerLayout->addWidget(user_combo_);
    auto *createUserButton = new QPushButton(QString::fromUtf8(u8"＋"), header);
    createUserButton->setToolTip(QString::fromUtf8(u8"新建用户"));
    createUserButton->setFixedWidth(38);
    headerLayout->addWidget(createUserButton);

    headerLayout->addWidget(new QLabel(QString::fromUtf8(u8"对弈难度"), header));
    auto *difficultyCombo = new QComboBox(header);
    difficultyCombo->addItem(QString::fromUtf8(u8"\u5165\u95e8"));
    difficultyCombo->addItem(QString::fromUtf8(u8"\u521d\u7ea7"));
    difficultyCombo->addItem(QString::fromUtf8(u8"\u4e2d\u7ea7"));
    difficultyCombo->addItem(QString::fromUtf8(u8"\u9ad8\u7ea7"));
    difficultyCombo->setCurrentIndex(1);
    difficultyCombo->setMinimumWidth(100);
    headerLayout->addWidget(difficultyCombo);

    auto *undoButton = new QPushButton(QString::fromUtf8(u8"悔棋"), header);
    undoButton->setProperty("secondary", true);
    auto *trainingButton = new QPushButton(QString::fromUtf8(u8"专项训练"), header);
    trainingButton->setProperty("secondary", true);
    auto *deepSeekButton = new QPushButton(QString::fromUtf8(u8"AI 设置"), header);
    deepSeekButton->setProperty("secondary", true);
    auto *newGameButton = new QPushButton(QString::fromUtf8(u8"新对局"), header);
    newGameButton->setObjectName("primaryButton");
    headerLayout->addWidget(undoButton);
    headerLayout->addWidget(trainingButton);
    headerLayout->addWidget(deepSeekButton);
    headerLayout->addWidget(newGameButton);
    rootLayout->addWidget(header);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(8);

    auto *boardCard = new QFrame(splitter);
    boardCard->setObjectName("contentCard");
    auto *boardLayout = new QVBoxLayout(boardCard);
    boardLayout->setContentsMargins(12, 12, 12, 12);
    board_widget_ = new XiangqiBoardWidget(boardCard);
    boardLayout->addWidget(board_widget_);

    auto *panel = new QFrame(splitter);
    panel->setObjectName("contentCard");
    panel->setMinimumWidth(350);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 14, 16, 14);
    panelLayout->setSpacing(10);

    auto *statusTitle = new QLabel(QString::fromUtf8(u8"系统状态"), panel);
    statusTitle->setObjectName("sectionTitle");
    panelLayout->addWidget(statusTitle);

    engine_status_label_ = new QLabel(QString::fromUtf8(u8"正在启动皮卡鱼……"), panel);
    engine_status_label_->setWordWrap(true);
    engine_status_label_->setObjectName("statusPill");
    panelLayout->addWidget(engine_status_label_);

    coach_status_label_ = new QLabel(QString::fromUtf8(u8"正在检查 DeepSeek 配置……"), panel);
    coach_status_label_->setWordWrap(true);
    coach_status_label_->setObjectName("statusPill");
    panelLayout->addWidget(coach_status_label_);

    auto *tabs = new QTabWidget(panel);
    auto *analysisTab = new QWidget(tabs);
    auto *analysisLayout = new QVBoxLayout(analysisTab);
    analysisLayout->setContentsMargins(0, 10, 0, 0);
    analysis_browser_ = new QTextBrowser(analysisTab);
    analysis_browser_->setPlaceholderText(QString::fromUtf8(
        u8"你执红方。每走一步，系统会显示推荐走法、局面损失和训练建议。"));
    analysisLayout->addWidget(analysis_browser_);
    tabs->addTab(analysisTab, QString::fromUtf8(u8"实时复盘"));

    auto *statsTab = new QWidget(tabs);
    auto *statsLayout = new QVBoxLayout(statsTab);
    statsLayout->setContentsMargins(0, 6, 0, 0);
    auto *statsScroll = new QScrollArea(statsTab);
    statsScroll->setWidgetResizable(true);
    statsScroll->setFrameShape(QFrame::NoFrame);
    auto *statsContent = new QWidget(statsScroll);
    auto *statsContentLayout = new QVBoxLayout(statsContent);
    statsContentLayout->setContentsMargins(2, 2, 2, 2);
    statsContentLayout->setSpacing(10);
    profile_dashboard_ = new ProfileDashboardWidget(statsContent);
    statsContentLayout->addWidget(profile_dashboard_);
    stats_browser_ = new QTextBrowser(statsContent);
    stats_browser_->setMinimumHeight(220);
    statsContentLayout->addWidget(stats_browser_);

    database_label_ = new QLabel(statsContent);
    database_label_->setWordWrap(true);
    database_label_->setObjectName("databasePath");
    database_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statsContentLayout->addWidget(database_label_);
    statsScroll->setWidget(statsContent);
    statsLayout->addWidget(statsScroll);
    tabs->addTab(statsTab, QString::fromUtf8(u8"个人统计"));
    panelLayout->addWidget(tabs, 1);

    splitter->addWidget(boardCard);
    splitter->addWidget(panel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({760, 430});
    rootLayout->addWidget(splitter, 1);
    setCentralWidget(central);
    setWindowTitle(QString::fromUtf8(u8"象棋个性化训练系统"));
    setMinimumSize(1020, 700);
    resize(1260, 800);

    setStyleSheet(QString::fromUtf8(R"(
        QMainWindow, QWidget#appRoot { background: #f4f1e9; color: #26231f; }
        QFrame#headerCard, QFrame#contentCard {
            background: #fffdf8;
            border: 1px solid #ddd5c6;
            border-radius: 10px;
        }
        QLabel#subtitle { color: #756d62; font-size: 12px; }
        QLabel#sectionTitle { font-size: 15px; font-weight: 600; }
        QLabel#statusPill {
            background: #f3efe6; border: 1px solid #e0d8c8;
            border-radius: 6px; padding: 7px 9px; color: #625b51;
        }
        QLabel#databasePath { color: #777067; font-size: 11px; padding-top: 4px; }
        QPushButton {
            min-height: 32px; padding: 0 14px; border-radius: 6px;
            border: 1px solid #cfc6b7; background: #fffdf8;
        }
        QPushButton:hover { background: #f0eadf; border-color: #b9ad9a; }
        QPushButton:pressed { background: #e6ded1; }
        QPushButton#primaryButton {
            color: white; background: #9b3f2f; border-color: #873426; font-weight: 600;
        }
        QPushButton#primaryButton:hover { background: #ad4937; }
        QComboBox {
            min-height: 32px; padding: 0 9px; border: 1px solid #cfc6b7;
            border-radius: 6px; background: white;
        }
        QTabWidget::pane { border: 1px solid #ddd5c6; border-radius: 6px; background: white; }
        QTabBar::tab { padding: 8px 16px; color: #665f55; }
        QTabBar::tab:selected { color: #8f382b; font-weight: 600; }
        QTextBrowser { border: none; background: white; padding: 8px; }
        QSplitter::handle { background: transparent; }
    )"));

    analyzer_ = new PikafishAnalyzer(this);
    coach_ = new DeepSeekCoach(this);
    connect(board_widget_, &XiangqiBoardWidget::moveCompleted,
            this, &MainWindow::handleMoveCompleted);
    connect(board_widget_, &XiangqiBoardWidget::gameEnded,
            this, &MainWindow::handleGameEnded);
    connect(analyzer_, &PikafishAnalyzer::analysisReady,
            this, &MainWindow::handleAnalysis);
    connect(analyzer_, &PikafishAnalyzer::analysisQueueDrained,
            this, &MainWindow::requestPendingGameReviews);
    connect(analyzer_, &PikafishAnalyzer::statusChanged,
            this, [this](const QString &message, bool available) {
                engine_status_label_->setText(message);
                engine_status_label_->setStyleSheet(available
                    ? "color:#276b3b; background:#edf7ef; border:1px solid #b9ddc1; border-radius:6px; padding:7px 9px;"
                    : "color:#9b342b; background:#fff0ed; border:1px solid #edc1ba; border-radius:6px; padding:7px 9px;");
            });
    connect(coach_, &DeepSeekCoach::coachingReady,
            this, &MainWindow::handleCoaching);
    connect(coach_, &DeepSeekCoach::gameReviewReady,
            this, &MainWindow::handleGameReview);
    connect(coach_, &DeepSeekCoach::connectionTested,
            this, [this](bool success, const QString &) {
                if (success) requestPendingGameReviews();
            });
    connect(coach_, &DeepSeekCoach::statusChanged,
            this, [this](const QString &message, bool available) {
                coach_status_label_->setText(message);
                coach_status_label_->setStyleSheet(available
                    ? "color:#276b3b; background:#edf7ef; border:1px solid #b9ddc1; border-radius:6px; padding:7px 9px;"
                    : "color:#805b16; background:#fff8e7; border:1px solid #ead7a6; border-radius:6px; padding:7px 9px;");
            });
    connect(newGameButton, &QPushButton::clicked,
            this, &MainWindow::startNewGame);
    connect(undoButton, &QPushButton::clicked,
            this, &MainWindow::undoTurn);
    connect(trainingButton, &QPushButton::clicked,
            this, &MainWindow::startPersonalTraining);
    connect(createUserButton, &QPushButton::clicked,
            this, &MainWindow::createUser);
    connect(user_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::switchUser);
    connect(deepSeekButton, &QPushButton::clicked,
            this, &MainWindow::configureDeepSeek);
    connect(difficultyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                board_widget_->setDifficulty(
                    static_cast<XiangqiBoardWidget::Difficulty>(index));
            });

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
    active_user_id_ = database_.selectedUserId();
    populateUsers();
    current_game_id_ = database_.startGame(active_user_id_, &error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建对局"), error);
    }
    database_.generateTrainingPositions(active_user_id_);
    refreshStats();
    showMilestoneReportIfNeeded();
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
        return;
    }
    pending_game_reviews_.insert(current_game_id_);
    requestPendingGameReviews();
    refreshStats();
    showMilestoneReportIfNeeded();
}

void MainWindow::handleAnalysis(const PikafishAnalyzer::AnalysisResult &result)
{
    if (!isCurrentMove(result.gameId, result.ply, result.actualMove)) {
        return;
    }

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
        coach_->requestCoaching(result, database_.trainingStats(active_user_id_));
    }
}

void MainWindow::handleCoaching(const DeepSeekCoach::CoachingResult &result)
{
    if (!isCurrentMove(result.gameId, result.ply, result.actualMove)) {
        return;
    }

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

void MainWindow::requestPendingGameReviews()
{
    if (!coach_ || !coach_->isConfigured() || !analyzer_
        || analyzer_->hasPendingAnalysis()) {
        return;
    }

    const QList<qint64> pending = pending_game_reviews_.values();
    for (qint64 gameId : pending) {
        if (database_.hasGameReview(gameId)) {
            pending_game_reviews_.remove(gameId);
            continue;
        }
        GameDatabase::GameReviewContext context;
        QString error;
        if (!database_.buildGameReviewContext(gameId, &context, &error)) {
            pending_game_reviews_.remove(gameId);
            coach_status_label_->setText(QString::fromUtf8(u8"无法准备整盘复盘：") + error);
            continue;
        }
        coach_->requestGameReview(context, database_.trainingStats(context.userId));
        pending_game_reviews_.remove(gameId);
    }
}

void MainWindow::handleGameReview(const DeepSeekCoach::GameReviewResult &result)
{
    GameDatabase::GameReviewContext currentContext;
    QString error;
    if (!database_.buildGameReviewContext(result.gameId, &currentContext, &error)
        || currentContext.userId != result.userId) {
        return;
    }

    GameDatabase::GameReview review;
    review.gameId = result.gameId;
    review.model = result.model;
    review.overview = result.overview;
    review.turningPoints = result.turningPoints;
    review.strengths = result.strengths;
    review.recurringPattern = result.recurringPattern;
    review.trainingPlan = result.trainingPlan;
    review.reflectionQuestion = result.reflectionQuestion;
    if (!database_.recordGameReview(review, &error)) {
        coach_status_label_->setText(QString::fromUtf8(u8"保存整盘复盘失败：") + error);
        return;
    }

    if (result.gameId != current_game_id_ || result.userId != active_user_id_) {
        return;
    }
    auto htmlText = [](QString text) {
        return text.toHtmlEscaped().replace("\n", "<br>");
    };
    const QString html = QString(
        "<div style='border:1px solid #cbb9e8; background:#f8f5ff; border-radius:8px; "
        "padding:12px; margin:14px 0'>"
        "<h3 style='color:#513a86; margin-top:0'>整盘 AI 复盘</h3>"
        "<b>总体评价</b><br>%1<br><br>"
        "<b>关键转折点</b><br>%2<br><br>"
        "<b>做得好的地方</b><br>%3<br><br>"
        "<b>可能重复的思考模式</b><br>%4<br><br>"
        "<b>下一阶段训练计划</b><br>%5<br><br>"
        "<b>复盘问题</b><br>%6</div>")
        .arg(htmlText(result.overview), htmlText(result.turningPoints),
             htmlText(result.strengths), htmlText(result.recurringPattern),
             htmlText(result.trainingPlan), htmlText(result.reflectionQuestion));
    analysis_browser_->append(html);
    QMessageBox::information(
        this, QString::fromUtf8(u8"整盘 AI 复盘已完成"),
        QString::fromUtf8(u8"本局的整体分析已经生成并保存，请在“走法分析”中查看。"));
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

    QString error;
    if (current_game_id_ >= 0 &&
        board_widget_->game().result() == XiangqiGame::GameResult::Ongoing) {
        database_.abandonGame(current_game_id_, &error);
    }
    board_widget_->newGame();
    analysis_browser_->clear();
    current_game_id_ = database_.startGame(active_user_id_, &error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建新对局"), error);
    }
    refreshStats();
}

void MainWindow::undoTurn()
{
    const int undone = board_widget_->undoTurn();
    if (undone == 0) {
        QMessageBox::information(this,
                                 QString::fromUtf8(u8"无法悔棋"),
                                 QString::fromUtf8(u8"当前还没有可以撤销的走法。"));
        return;
    }

    QString error;
    const int lastKeptPly = static_cast<int>(board_widget_->game().moveHistory().size());
    if (current_game_id_ >= 0 &&
        !database_.truncateGame(current_game_id_, lastKeptPly, &error)) {
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"悔棋记录错误"),
                             QString::fromUtf8(u8"棋盘已经退回，但数据库同步失败：") + error);
    }

    analysis_browser_->clear();
    analysis_browser_->append(QString::fromUtf8(
        u8"<p style='color:#555'>已悔棋，撤销 %1 步。后续分析将以当前局面为准。</p>")
        .arg(undone));
    refreshStats();
}

void MainWindow::startPersonalTraining()
{
    TrainingDialog dialog(&database_, active_user_id_, this);
    dialog.exec();
    refreshStats();
}

void MainWindow::populateUsers()
{
    const QSignalBlocker blocker(user_combo_);
    user_combo_->clear();
    const QVector<GameDatabase::User> users = database_.users();
    int selectedIndex = 0;
    for (const auto &user : users) {
        user_combo_->addItem(user.name, user.id);
        if (user.id == active_user_id_) {
            selectedIndex = user_combo_->count() - 1;
        }
    }
    user_combo_->setCurrentIndex(selectedIndex);
}

void MainWindow::createUser()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, QString::fromUtf8(u8"新建用户"),
        QString::fromUtf8(u8"请输入用户名："), QLineEdit::Normal,
        QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    QString error;
    const qint64 userId = database_.createUser(name, &error);
    if (userId < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建用户"), error);
        return;
    }
    populateUsers();
    const int index = user_combo_->findData(userId);
    if (index >= 0) {
        user_combo_->setCurrentIndex(index);
    }
}

void MainWindow::switchUser(int comboIndex)
{
    if (comboIndex < 0) {
        return;
    }
    const qint64 newUserId = user_combo_->itemData(comboIndex).toLongLong();
    if (newUserId <= 0 || newUserId == active_user_id_) {
        return;
    }

    if (!board_widget_->game().moveHistory().empty() &&
        board_widget_->game().result() == XiangqiGame::GameResult::Ongoing) {
        const auto answer = QMessageBox::question(
            this, QString::fromUtf8(u8"切换用户"),
            QString::fromUtf8(u8"切换用户会结束当前未完成的对局，确定继续吗？"));
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker blocker(user_combo_);
            user_combo_->setCurrentIndex(user_combo_->findData(active_user_id_));
            return;
        }
    }

    QString error;
    if (current_game_id_ >= 0 &&
        board_widget_->game().result() == XiangqiGame::GameResult::Ongoing) {
        database_.abandonGame(current_game_id_, &error);
    }
    if (!database_.setSelectedUserId(newUserId, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"切换用户失败"), error);
        const QSignalBlocker blocker(user_combo_);
        user_combo_->setCurrentIndex(user_combo_->findData(active_user_id_));
        return;
    }
    active_user_id_ = newUserId;

    board_widget_->newGame();
    analysis_browser_->clear();
    current_game_id_ = database_.startGame(active_user_id_, &error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建用户对局"), error);
    }
    database_.generateTrainingPositions(active_user_id_);
    refreshStats();
    showMilestoneReportIfNeeded();
}

void MainWindow::showMilestoneReportIfNeeded()
{
    bool created = false;
    QString error;
    const GameDatabase::ProfileReport report =
        database_.generateMilestoneReport(active_user_id_, &created, &error);
    if (!error.isEmpty()) {
        engine_status_label_->setText(QString::fromUtf8(u8"生成阶段总结失败：") + error);
        return;
    }
    if (created && report.id >= 0) {
        QMessageBox::information(
            this,
            QString::fromUtf8(u8"个人阶段总结 · 第 %1 盘").arg(report.throughGames),
            report.summary);
        refreshStats();
    }
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
    const GameDatabase::TrainingStats stats = database_.trainingStats(active_user_id_);
    const GameDatabase::TrainingSummary training = database_.trainingSummary(active_user_id_);
    const GameDatabase::UserProfile profile = database_.userProfile(active_user_id_);
    const QVector<GameDatabase::ProfileReport> reports =
        database_.profileReports(active_user_id_);
    QString latestReport = QString::fromUtf8(u8"完成 10 盘有效对局后生成第一份阶段总结。");
    if (!reports.isEmpty()) {
        latestReport = reports.front().summary.toHtmlEscaped();
        latestReport.replace("\n", "<br>");
    }
    const QString userName = user_combo_ ? user_combo_->currentText() : QString();
    profile_dashboard_->setProfileData(
        userName, profile, stats, training,
        database_.recentGamePerformance(active_user_id_, 10));
    stats_browser_->setHtml(QString::fromUtf8(
        u8"<h3>%1 的训练说明</h3>"
        u8"<b>当前训练重点</b><br>%2<br><br>"
        u8"个人错题：%3　今日到期：%4　AI讲解：%5<hr>"
        u8"<b>最近阶段总结</b><br>%6")
        .arg(userName.toHtmlEscaped(),
             profile.mainWeakness.toHtmlEscaped())
        .arg(training.positions)
        .arg(training.due)
        .arg(stats.coachedMoves)
        .arg(latestReport));
}

bool MainWindow::isCurrentMove(qint64 gameId, int ply, const QString &uciMove) const
{
    if (gameId != current_game_id_ || ply <= 0) {
        return false;
    }
    const auto &history = board_widget_->game().moveHistory();
    if (ply > static_cast<int>(history.size())) {
        return false;
    }

    const XiangqiGame::MoveRecord &move = history[static_cast<std::size_t>(ply - 1)];
    auto square = [](int row, int col) {
        return QString(QChar('a' + col)) + QChar('9' - row);
    };
    const QString currentUci = square(move.fromRow, move.fromCol) +
                               square(move.toRow, move.toCol);
    return currentUci == uciMove;
}
