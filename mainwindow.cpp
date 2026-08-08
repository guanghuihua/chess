#include "mainwindow.h"

#include "./ui_mainwindow.h"
#include "xiangqi_board_widget.h"
#include "training_dialog.h"
#include "profile_dashboard_widget.h"
#include "game_review_dialog.h"
#include "chess_score_importer.h"
#include "engine_variation_dialog.h"

#include <algorithm>
#include <QFont>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QUuid>
#include <QUrl>

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
    auto *resignButton = new QPushButton(QString::fromUtf8(u8"认输"), header);
    resignButton->setProperty("danger", true);
    auto *trainingButton = new QPushButton(QString::fromUtf8(u8"专项训练"), header);
    trainingButton->setProperty("secondary", true);
    auto *reviewButton = new QPushButton(QString::fromUtf8(u8"对局复盘"), header);
    reviewButton->setProperty("secondary", true);
    auto *favoriteButton = new QPushButton(QString::fromUtf8(u8"收藏棋谱"), header);
    favoriteButton->setProperty("secondary", true);
    auto *favoritesButton = new QPushButton(QString::fromUtf8(u8"收藏夹"), header);
    favoritesButton->setProperty("secondary", true);
    auto *aiSettingsButton = new QPushButton(QString::fromUtf8(u8"AI 设置"), header);
    aiSettingsButton->setProperty("secondary", true);
    auto *newGameButton = new QPushButton(QString::fromUtf8(u8"新对局"), header);
    newGameButton->setObjectName("primaryButton");
    headerLayout->addWidget(undoButton);
    headerLayout->addWidget(resignButton);
    headerLayout->addWidget(trainingButton);
    headerLayout->addWidget(reviewButton);
    headerLayout->addWidget(favoriteButton);
    headerLayout->addWidget(favoritesButton);
    headerLayout->addWidget(aiSettingsButton);
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
    panel->setMinimumWidth(430);
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

    tabs_ = new QTabWidget(panel);

    auto *coachTab = new QWidget(tabs_);
    auto *coachLayout = new QVBoxLayout(coachTab);
    coachLayout->setContentsMargins(0, 10, 0, 0);
    auto *coachHint = new QLabel(QString::fromUtf8(
        u8"AI 教练会把引擎结论转化为可执行的改进建议；对局结束后，这里会显示整盘总结。"),
        coachTab);
    coachHint->setWordWrap(true);
    coachHint->setObjectName("coachHint");
    coachLayout->addWidget(coachHint);
    auto *insightSplitter = new QSplitter(Qt::Vertical, coachTab);
    insightSplitter->setChildrenCollapsible(false);
    auto *analysisPane = new QWidget(insightSplitter);
    analysisPane->setObjectName("insightCard");
    auto *analysisPaneLayout = new QVBoxLayout(analysisPane);
    analysisPaneLayout->setContentsMargins(0, 0, 0, 0);
    auto *analysisTitle = new QLabel(QString::fromUtf8(u8"Pikafish 引擎结论"), analysisPane);
    analysisTitle->setObjectName("sectionTitle");
    analysis_browser_ = new QTextBrowser(analysisPane);
    analysis_browser_->setObjectName("engineAnalysisBrowser");
    analysis_browser_->setOpenLinks(false);
    analysis_browser_->setPlaceholderText(QString::fromUtf8(
        u8"你执红方。每走一步，这里会显示推荐走法、评分和推荐变化。"));
    connect(analysis_browser_, &QTextBrowser::anchorClicked, this,
            [this](const QUrl &url) {
                if (url.scheme() != QStringLiteral("recommendation")) return;
                bool ok = false;
                const int ply = url.toString().section(':', 1).toInt(&ok);
                if (ok) showEngineRecommendation(ply);
            });
    analysisPaneLayout->addWidget(analysisTitle);
    analysisPaneLayout->addWidget(analysis_browser_, 1);

    auto *advicePane = new QWidget(insightSplitter);
    advicePane->setObjectName("insightCard");
    auto *advicePaneLayout = new QVBoxLayout(advicePane);
    advicePaneLayout->setContentsMargins(0, 0, 0, 0);
    auto *adviceTitle = new QLabel(QString::fromUtf8(u8"AI 教练解释与对话"), advicePane);
    adviceTitle->setObjectName("sectionTitle");
    advice_scroll_ = new QScrollArea(advicePane);
    advice_scroll_->setObjectName("adviceScroll");
    advice_scroll_->setWidgetResizable(true);
    advice_scroll_->setFrameShape(QFrame::NoFrame);
    advice_feed_ = new QWidget(advice_scroll_);
    advice_feed_->setObjectName("adviceFeed");
    advice_feed_layout_ = new QVBoxLayout(advice_feed_);
    advice_feed_layout_->setContentsMargins(5, 5, 5, 5);
    advice_feed_layout_->setSpacing(10);
    advice_feed_layout_->setAlignment(Qt::AlignTop);
    advice_scroll_->setWidget(advice_feed_);
    advicePaneLayout->addWidget(adviceTitle);
    advicePaneLayout->addWidget(advice_scroll_, 1);
    insightSplitter->addWidget(analysisPane);
    insightSplitter->addWidget(advicePane);
    insightSplitter->setSizes({190, 400});
    coachLayout->addWidget(insightSplitter, 1);
    coach_thought_edit_ = new QLineEdit(coachTab);
    coach_thought_edit_->setPlaceholderText(QString::fromUtf8(
        u8"落子前可写下你的思路（可选），例如：我只看到兑车，没算黑方的反击"));
    coachLayout->addWidget(coach_thought_edit_);
    auto *questionRow = new QHBoxLayout;
    coach_question_edit_ = new QLineEdit(coachTab);
    coach_question_edit_->setPlaceholderText(QString::fromUtf8(
        u8"追问 AI 教练，例如：这一步为什么会丢子？我当时应该先检查什么？"));
    coach_question_button_ = new QPushButton(QString::fromUtf8(u8"发送追问"), coachTab);
    coach_question_button_->setObjectName("primaryButton");
    questionRow->addWidget(coach_question_edit_, 1);
    questionRow->addWidget(coach_question_button_);
    coachLayout->addLayout(questionRow);
    tabs_->addTab(coachTab, QString::fromUtf8(u8"分析与教练"));

    auto *statsTab = new QWidget(tabs_);
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
    tabs_->addTab(statsTab, QString::fromUtf8(u8"个人统计"));
    panelLayout->addWidget(tabs_, 1);

    splitter->addWidget(boardCard);
    splitter->addWidget(panel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({760, 520});
    rootLayout->addWidget(splitter, 1);
    setCentralWidget(central);
    setWindowTitle(QString::fromUtf8(u8"象棋个性化训练系统"));
    setMinimumSize(1020, 700);
    resize(1380, 860);

    setStyleSheet(QString::fromUtf8(R"(
        QMainWindow, QWidget#appRoot {
            background: #f4f1e9; color: #26231f;
            font-family: "Microsoft YaHei UI", "Noto Sans CJK SC", "Segoe UI";
        }
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
        QLabel#coachHint {
            color: #695b46; background: #f7f1e5; border: 1px solid #eadfc9;
            border-radius: 6px; padding: 9px; font-size: 12px;
        }
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
        QPushButton[danger="true"] {
            color: #9b342b; background: #fff0ed; border-color: #e5b9b2;
        }
        QPushButton[danger="true"]:hover { background: #f8dcd7; }
        QComboBox {
            min-height: 32px; padding: 0 9px; border: 1px solid #cfc6b7;
            border-radius: 6px; background: white;
        }
        QTabWidget::pane { border: 1px solid #ddd5c6; border-radius: 6px; background: white; }
        QTabBar::tab { padding: 8px 16px; color: #665f55; }
        QTabBar::tab:selected { color: #8f382b; font-weight: 600; }
        QTextBrowser { border: none; background: white; padding: 8px; }
        QWidget#insightCard {
            background: #fffdf9; border: 1px solid #ddd5c6;
            border-radius: 8px; padding: 7px;
        }
        QTextBrowser#engineAnalysisBrowser {
            border: none; background: #fffdf9;
        }
        QScrollArea#adviceScroll, QWidget#adviceFeed { border: none; background: #f7f6f2; }
        QFrame#coachCard {
            background: #ffffff; border: 1px solid #d8d6d0;
            border-radius: 7px;
        }
        QFrame#coachCard[tone="coach"] { border-color: #aebbc6; }
        QFrame#coachCard[tone="review"] { border-color: #b9aa91; background: #fffdf8; }
        QFrame#coachCard[tone="notice"] { border-color: #d8d1c5; background: #faf9f6; }
        QFrame#coachCard[tone="error"] { border-color: #d7aaa4; background: #fff7f5; }
        QFrame#coachCard[undone="true"] { border-color: #aaa7a0; background: #f1f1ef; }
        QLabel#coachCardTitle { color: #26343d; font-size: 14px; font-weight: 700; }
        QLabel#coachCardLead { color: #20272c; font-size: 14px; font-weight: 600; }
        QLabel#coachCardStatus {
            color: #6e6254; background: #e7e3dc; border-radius: 4px;
            padding: 2px 6px; font-size: 11px;
        }
        QFrame#coachSection { background: #f6f7f7; border: none; border-radius: 5px; }
        QLabel#coachSectionTitle { color: #596873; font-size: 11px; font-weight: 700; }
        QLabel#coachSectionText { color: #30383d; font-size: 13px; }
        QFrame#chatUser { background: #e9edf0; border: none; border-radius: 7px; }
        QFrame#chatCoach { background: #ffffff; border: 1px solid #d8d6d0; border-radius: 7px; }
        QLabel#chatRole { color: #596873; font-size: 11px; font-weight: 700; }
        QLabel#chatText { color: #30383d; font-size: 13px; }
        QLineEdit {
            min-height: 34px; padding: 0 10px; border: 1px solid #cfc6b7;
            border-radius: 7px; background: white; selection-background-color:#c9b8ea;
        }
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
    connect(coach_, &DeepSeekCoach::chatReplyReady,
            this, &MainWindow::handleChatReply);
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
    connect(coach_question_button_, &QPushButton::clicked,
            this, &MainWindow::sendCoachQuestion);
    connect(coach_question_edit_, &QLineEdit::returnPressed,
            this, &MainWindow::sendCoachQuestion);
    connect(resignButton, &QPushButton::clicked,
            this, &MainWindow::resignGame);
    connect(trainingButton, &QPushButton::clicked,
            this, &MainWindow::startPersonalTraining);
    connect(reviewButton, &QPushButton::clicked,
            this, &MainWindow::openGameReview);
    connect(favoriteButton, &QPushButton::clicked,
            this, &MainWindow::favoriteScore);
    connect(favoritesButton, &QPushButton::clicked,
            this, &MainWindow::openFavoriteScores);
    connect(createUserButton, &QPushButton::clicked,
            this, &MainWindow::createUser);
    connect(user_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::switchUser);
    connect(aiSettingsButton, &QPushButton::clicked,
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

QFrame *MainWindow::appendAdviceCard(
    const QString &title, const QString &lead,
    const QStringList &sectionTitles, const QStringList &sectionTexts,
    const QString &tone, int ply)
{
    if (!advice_feed_layout_) return nullptr;
    auto *card = new QFrame(advice_feed_);
    card->setObjectName("coachCard");
    card->setProperty("tone", tone);
    card->setProperty("ply", ply);
    card->setProperty("undone", false);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 11);
    layout->setSpacing(7);

    auto *header = new QHBoxLayout;
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("coachCardTitle");
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *status = new QLabel(card);
    status->setObjectName("coachCardStatus");
    status->setText(QString::fromUtf8(u8"已撤销分支 · 分析保留"));
    status->setVisible(false);
    header->addWidget(titleLabel, 1);
    header->addWidget(status, 0, Qt::AlignTop);
    layout->addLayout(header);

    if (!lead.trimmed().isEmpty()) {
        auto *leadLabel = new QLabel(lead, card);
        leadLabel->setObjectName("coachCardLead");
        leadLabel->setWordWrap(true);
        leadLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(leadLabel);
    }

    const int count = std::min(sectionTitles.size(), sectionTexts.size());
    for (int i = 0; i < count; ++i) {
        if (sectionTexts.at(i).trimmed().isEmpty()) continue;
        auto *section = new QFrame(card);
        section->setObjectName("coachSection");
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(9, 7, 9, 8);
        sectionLayout->setSpacing(3);
        auto *sectionTitle = new QLabel(sectionTitles.at(i), section);
        sectionTitle->setObjectName("coachSectionTitle");
        auto *sectionText = new QLabel(sectionTexts.at(i), section);
        sectionText->setObjectName("coachSectionText");
        sectionText->setWordWrap(true);
        sectionText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sectionLayout->addWidget(sectionTitle);
        sectionLayout->addWidget(sectionText);
        layout->addWidget(section);
    }
    advice_feed_layout_->addWidget(card);
    scrollAdviceToBottom();
    return card;
}

void MainWindow::appendCoachFeedbackControls(QFrame *card, qint64 gameId, int ply)
{
    if (!card || gameId < 0 || ply <= 0) return;
    auto *row = new QFrame(card);
    row->setObjectName("coachFeedback");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 3, 0, 0);
    layout->setSpacing(6);
    auto *label = new QLabel(QString::fromUtf8(u8"这条讲解："), row);
    layout->addWidget(label);
    const QList<QPair<QString, QString>> options = {
        {QStringLiteral("clear"), QString::fromUtf8(u8"说清楚了")},
        {QStringLiteral("abstract"), QString::fromUtf8(u8"太抽象")},
        {QStringLiteral("variation_unclear"), QString::fromUtf8(u8"不理解变化")}};
    for (const auto &option : options) {
        auto *button = new QPushButton(option.second, row);
        button->setObjectName("coachFeedbackButton");
        connect(button, &QPushButton::clicked, this,
                [this, row, label, gameId, ply, feedback = option.first, text = option.second] {
            QString error;
            if (!database_.recordCoachFeedback(active_user_id_, gameId, ply, feedback, &error)) {
                label->setText(QString::fromUtf8(u8"保存失败：") + error);
                return;
            }
            label->setText(QString::fromUtf8(u8"已记录：") + text);
            for (QPushButton *other : row->findChildren<QPushButton *>()) {
                other->setEnabled(false);
            }
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    if (auto *cardLayout = qobject_cast<QVBoxLayout *>(card->layout())) {
        cardLayout->addWidget(row);
    }
}

void MainWindow::appendChatBubble(bool user, const QString &text, bool error)
{
    if (!advice_feed_layout_) return;
    auto *bubble = new QFrame(advice_feed_);
    bubble->setObjectName(user ? "chatUser" : "chatCoach");
    if (error) bubble->setStyleSheet("background:#fff7f5;border:1px solid #d7aaa4;");
    auto *layout = new QVBoxLayout(bubble);
    layout->setContentsMargins(10, 8, 10, 9);
    layout->setSpacing(3);
    auto *role = new QLabel(user ? QString::fromUtf8(u8"你")
                                 : (error ? QString::fromUtf8(u8"请求失败")
                                          : QString::fromUtf8(u8"AI 教练")), bubble);
    role->setObjectName("chatRole");
    auto *content = new QLabel(text, bubble);
    content->setObjectName("chatText");
    content->setWordWrap(true);
    content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(role);
    layout->addWidget(content);
    advice_feed_layout_->addWidget(bubble, 0, user ? Qt::AlignRight : Qt::AlignLeft);
    bubble->setMaximumWidth(430);
    scrollAdviceToBottom();
}

void MainWindow::clearAdviceCards()
{
    if (!advice_feed_layout_) return;
    while (QLayoutItem *item = advice_feed_layout_->takeAt(0)) {
        if (QWidget *widget = item->widget()) widget->deleteLater();
        delete item;
    }
}

void MainWindow::markUndoneAdviceCards(int lastKeptPly)
{
    if (!advice_feed_) return;
    const auto cards = advice_feed_->findChildren<QFrame *>("coachCard",
                                                            Qt::FindDirectChildrenOnly);
    for (QFrame *card : cards) {
        const int ply = card->property("ply").toInt();
        if (ply <= lastKeptPly || ply <= 0) continue;
        card->setProperty("undone", true);
        if (auto *status = card->findChild<QLabel *>("coachCardStatus")) {
            status->setVisible(true);
        }
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
}

void MainWindow::scrollAdviceToBottom()
{
    if (!advice_scroll_) return;
    QTimer::singleShot(0, advice_scroll_, [this] {
        if (advice_scroll_) advice_scroll_->verticalScrollBar()->setValue(
            advice_scroll_->verticalScrollBar()->maximum());
    });
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
    appendAdviceCard(QString::fromUtf8(u8"等待首个关键决策"),
                     QString::fromUtf8(u8"走棋后，每个值得复盘的步骤会单独生成一张分析卡片。"),
                     {}, {}, QStringLiteral("notice"));
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
    if (!database_.finishGame(current_game_id_, board_widget_->game().result(),
                              current_game_end_reason_, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"保存结果失败"), error);
        return;
    }
    pending_game_reviews_.insert(current_game_id_);
    database_.generateTrainingPositions(active_user_id_);
    if (advice_feed_layout_) {
        const QString progress = coach_ && coach_->isConfigured()
            ? QString::fromUtf8(u8"正在等待引擎完成剩余分析，然后生成整盘 AI 建议……")
            : QString::fromUtf8(u8"引擎分析会继续保存；配置 DeepSeek 后可生成整盘 AI 建议。");
        appendAdviceCard(QString::fromUtf8(u8"本局已经结束"), progress,
                         {}, {}, QStringLiteral("notice"));
    }
    if (tabs_) tabs_->setCurrentIndex(0);
    requestPendingGameReviews();
    refreshStats();
    showMilestoneReportIfNeeded();
}

void MainWindow::handleAnalysis(const PikafishAnalyzer::AnalysisResult &result)
{
    if (!isCurrentMove(result.gameId, result.ply, result.actualMove)) {
        bool matchedUndo = false;
        QString undoError;
        if (!database_.attachAnalysisToUndoEvent(
                result.gameId, result.ply, result.actualMove, result.bestMove,
                result.scoreLoss, result.category, result.principalVariation,
                &matchedUndo, &undoError)) {
            engine_status_label_->setText(QString::fromUtf8(u8"保存悔棋分析失败：") + undoError);
        }
        if (matchedUndo && result.scoreLoss > 30) {
            coach_->requestCoaching(
                result, database_.trainingStats(active_user_id_),
                QString());
        }
        return;
    }

    QString error;
    if (!database_.recordAnalysis(result.gameId, result.ply, result.actualMove,
                                  result.bestMove, result.bestScore, result.actualScore,
                                  result.scoreLoss, result.category,
                                  result.principalVariation, result.rawPrincipalVariation,
                                  &error)) {
        engine_status_label_->setText(QString::fromUtf8(u8"保存分析失败：") + error);
    }
    database_.generateTrainingPositions(active_user_id_);
    current_analyses_.insert(result.ply, result);

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
        "<div style='background:#fbf8f1;border:1px solid #e2d9ca;"
        "padding:10px;margin:5px 0 10px 0'>"
        "<div style='color:#756b5d;font-size:12px'>第 %1 步</div>"
        "<div style='font-size:15px;margin:3px 0 6px 0;color:%2'><b>%3</b></div>"
        "<b>实战</b>　%4<br>"
        "<b>推荐</b>　<span style='color:#276b3b'>%5</span><br>"
        "<span style='color:#756b5d'>局面评价下降 %6 分</span>　"
        "<a style='color:#245f73;text-decoration:none' href='recommendation:%7'>"
        "在棋盘上查看推荐着 →</a></div>")
        .arg(result.ply)
        .arg(color, categoryText)
        .arg(result.actualNotation.toHtmlEscaped())
        .arg(result.bestNotation.toHtmlEscaped())
        .arg(result.scoreLoss)
        .arg(result.ply);
    analysis_browser_->append(html);
    const QString actualLine = result.actualPrincipalVariation.isEmpty()
        ? QString::fromUtf8(u8"引擎未返回足够的后续变化")
        : result.actualPrincipalVariation;
    const QString bestLine = result.principalVariation.isEmpty()
        ? QString::fromUtf8(u8"引擎未返回足够的后续变化")
        : result.principalVariation;
    const QString learnerThought = coach_thought_edit_ ? coach_thought_edit_->text().trimmed()
                                                       : QString();
    if (coach_thought_edit_ && !learnerThought.isEmpty()) {
        coach_thought_edit_->clear();
    }
    if (result.scoreLoss > 30) {
        appendAdviceCard(
            QString::fromUtf8(u8"第 %1 步 · Pikafish 双线对比").arg(result.ply),
            QString::fromUtf8(u8"实战 %1 使评价从 %2 变为 %3；推荐是 %4。")
                .arg(result.actualNotation).arg(result.bestScore).arg(result.actualScore)
                .arg(result.bestNotation),
            {QString::fromUtf8(u8"实战后：对手的最强回应"),
             QString::fromUtf8(u8"推荐着后：双方的最佳应对"),
             QString::fromUtf8(u8"你当时的思路")},
            {result.actualNotation + QString::fromUtf8(u8" -> ") + actualLine,
             bestLine,
             learnerThought.isEmpty() ? QString::fromUtf8(u8"未记录；下次落子前可在下方写下正在计算的变化。")
                                      : learnerThought},
            QStringLiteral("notice"), result.ply);
    }
    coach_chat_context_ = QString::fromUtf8(
        u8"第 %1 步，红方实际走法：%2\n"
        u8"Pikafish 推荐：%3\n评分：%4→%5，局面损失：%6，等级：%7\n"
        u8"实战后惩罚线：%8\n推荐着应对线：%9\n学习者当时思路：%10")
        .arg(result.ply)
        .arg(result.actualNotation, result.bestNotation)
        .arg(result.bestScore).arg(result.actualScore).arg(result.scoreLoss)
        .arg(categoryText, actualLine, bestLine,
             learnerThought.isEmpty() ? QString::fromUtf8(u8"未记录") : learnerThought);
    pending_chat_ply_ = result.ply;
    refreshStats();
    if (result.scoreLoss > 30) {
        coach_->requestCoaching(
            result, database_.trainingStats(active_user_id_),
            learnerThought);
    }
}

void MainWindow::handleCoaching(const DeepSeekCoach::CoachingResult &result)
{
    if (!isCurrentMove(result.gameId, result.ply, result.actualMove)) {
        bool matchedUndo = false;
        QString undoError;
        database_.attachCoachingToUndoEvent(
            result.gameId, result.ply, result.actualMove, result.diagnosis,
            result.evidence, result.trainingTask, result.reflectionQuestion,
            &matchedUndo, &undoError);
        if (!undoError.isEmpty()) {
            coach_status_label_->setText(QString::fromUtf8(u8"保存悔棋 AI 建议失败：") + undoError);
        }
        if (matchedUndo && result.gameId == current_game_id_) {
            QFrame *card = appendAdviceCard(
                QString::fromUtf8(u8"第 %1 步 · 悔棋分支分析").arg(result.ply),
                result.diagnosis,
                {QString::fromUtf8(u8"关键变化"), QString::fromUtf8(u8"推荐着的目的"),
                 QString::fromUtf8(u8"实战判定标准")},
                {result.evidence, result.trainingTask, result.reflectionQuestion},
                QStringLiteral("coach"), result.ply);
            if (card) {
                appendCoachFeedbackControls(card, result.gameId, result.ply);
                card->setProperty("undone", true);
                if (auto *status = card->findChild<QLabel *>("coachCardStatus")) {
                    status->setVisible(true);
                }
                card->style()->unpolish(card);
                card->style()->polish(card);
            }
        }
        return;
    }

    QString error;
    if (!database_.recordCoaching(result.gameId, result.ply, result.model,
                                  result.diagnosis, result.evidence,
                                  result.trainingTask, result.reflectionQuestion,
                                  &error)) {
        coach_status_label_->setText(QString::fromUtf8(u8"保存 AI 建议失败：") + error);
    }

    QFrame *card = appendAdviceCard(
        QString::fromUtf8(u8"第 %1 步 · AI 教练").arg(result.ply), result.diagnosis,
        {QString::fromUtf8(u8"关键变化"), QString::fromUtf8(u8"推荐着的目的"),
         QString::fromUtf8(u8"实战判定标准")},
        {result.evidence, result.trainingTask, result.reflectionQuestion},
        QStringLiteral("coach"), result.ply);
    appendCoachFeedbackControls(card, result.gameId, result.ply);
    coach_chat_context_ += QString::fromUtf8(
        u8"\nAI 初步诊断：%1\n关键变化：%2\n推荐着目的：%3\n实战判定标准：%4")
        .arg(result.diagnosis, result.evidence,
             result.trainingTask, result.reflectionQuestion);
    if (tabs_) tabs_->setCurrentIndex(0);
    refreshStats();
}

void MainWindow::requestPendingGameReviews()
{
    if (!analyzer_ || analyzer_->hasPendingAnalysis()) {
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
        if (coach_ && coach_->isConfigured()) {
            coach_->requestGameReview(context, database_.trainingStats(context.userId));
        } else {
            DeepSeekCoach::GameReviewResult localReview;
            localReview.gameId = context.gameId;
            localReview.userId = context.userId;
            localReview.model = QStringLiteral("local-engine-coach");
            localReview.overview = QString::fromUtf8(
                u8"本局共走 %1 步，红方 %2 步得到引擎分析，平均局面损失为 %3。"
                u8"这是依据引擎数据生成的离线总结；配置 DeepSeek 后可获得更深入的思考模式分析。")
                .arg(context.totalMoves).arg(context.analyzedMoves)
                .arg(context.averageLoss, 0, 'f', 1);
            localReview.turningPoints = context.keyMoments;
            localReview.strengths = context.blunders == 0
                ? QString::fromUtf8(u8"本局没有被引擎判定为严重失误的红方着法。")
                : QString::fromUtf8(u8"你完成了整盘对局，并留下了可用于针对训练的真实决策数据。");
            localReview.recurringPattern = context.undoSummary;
            localReview.trainingPlan = context.blunders > 0
                ? QString::fromUtf8(u8"1. 练习本局损失最大的局面；2. 落子前固定检查将军、吃子和直接威胁；3. 一周后再次测试同类局面。")
                : QString::fromUtf8(u8"1. 复查关键转折点；2. 比较实际着与引擎推荐着；3. 用一句话写下当时的候选着。");
            localReview.reflectionQuestion = QString::fromUtf8(
                u8"本局哪一步最能反映你的思考习惯？如果重新选择，你会先检查什么？");
            handleGameReview(localReview);
        }
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
    const QString reviewTitle = result.model == QStringLiteral("local-engine-coach")
        ? QString::fromUtf8(u8"整盘离线教练总结")
        : QString::fromUtf8(u8"整盘 AI 复盘");
    appendAdviceCard(
        reviewTitle, result.overview,
        {QString::fromUtf8(u8"悔棋专项证据"), QString::fromUtf8(u8"关键转折"),
         QString::fromUtf8(u8"做得好的地方"), QString::fromUtf8(u8"重复问题"),
         QString::fromUtf8(u8"下一阶段训练"), QString::fromUtf8(u8"复盘问题")},
        {currentContext.undoSummary, result.turningPoints, result.strengths,
         result.recurringPattern, result.trainingPlan, result.reflectionQuestion},
        QStringLiteral("review"));
    coach_chat_context_ = QString::fromUtf8(
        u8"这是第 %1 局的整盘复盘。\n结果：%2，结束原因：%3\n完整棋谱：\n%4\n\n"
        u8"关键转折点：\n%5\n\n悔棋证据：\n%6\n\n已有复盘：\n%7\n%8\n%9\n%10")
        .arg(currentContext.gameId).arg(currentContext.result, currentContext.endReason,
             currentContext.moveTranscript, currentContext.keyMoments,
             currentContext.undoSummary, result.overview, result.turningPoints,
             result.recurringPattern, result.trainingPlan);
    pending_chat_ply_ = 0;
    if (tabs_) tabs_->setCurrentIndex(0);
    coach_status_label_->setText(result.model == QStringLiteral("local-engine-coach")
        ? QString::fromUtf8(u8"整盘离线教练总结已生成并保存")
        : QString::fromUtf8(u8"整盘 AI 建议已生成并保存"));
    showGameReviewPopup(result, currentContext);
}

void MainWindow::showGameReviewPopup(
    const DeepSeekCoach::GameReviewResult &result,
    const GameDatabase::GameReviewContext &context)
{
    auto htmlText = [](QString text) {
        return text.toHtmlEscaped().replace("\n", "<br>");
    };
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8(u8"本局整盘复盘"));
    dialog.resize(820, 720);
    dialog.setMinimumSize(680, 560);
    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(QString::fromUtf8(u8"整盘训练报告"), &dialog);
    QFont titleFont = title->font();
    titleFont.setPointSize(19);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *subtitle = new QLabel(
        QString::fromUtf8(u8"引擎证据 + 完整棋谱 + 悔棋决策"), &dialog);
    subtitle->setStyleSheet("color:#756d62;margin-bottom:6px;");
    auto *browser = new QTextBrowser(&dialog);
    browser->setHtml(QString::fromUtf8(
        u8"<style>body{font-size:15px;line-height:1.6;color:#29251f}"
        u8".card{background:#fffdf8;border:1px solid #ddd5c6;border-radius:8px;"
        u8"padding:12px;margin:9px 0}.undo{background:#fff4df;border-color:#dfb77b}"
        u8"h3{color:#7f352a;margin:0 0 7px 0}</style>"
        u8"<div class='card'><h3>核心结论</h3>%1</div>"
        u8"<div class='card undo'><h3>悔棋专项分析</h3>%2</div>"
        u8"<div class='card'><h3>关键转折</h3>%3</div>"
        u8"<div class='card'><h3>重复问题</h3>%4</div>"
        u8"<div class='card'><h3>针对训练</h3>%5</div>"
        u8"<div class='card'><h3>复盘问题</h3>%6</div>")
        .arg(htmlText(result.overview), htmlText(context.undoSummary),
             htmlText(result.turningPoints), htmlText(result.recurringPattern),
             htmlText(result.trainingPlan), htmlText(result.reflectionQuestion)));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(browser, 1);
    layout->addWidget(buttons);
    dialog.setStyleSheet(
        "QDialog{background:#f4f1e9} QTextBrowser{background:#fffdf8;"
        "border:1px solid #d8cfbf;border-radius:9px;padding:12px}"
        "QPushButton{min-height:32px;padding:0 18px;border:1px solid #cfc6b7;"
        "border-radius:6px;background:#fffdf8}");
    dialog.exec();
}

void MainWindow::showEngineRecommendation(int ply)
{
    const auto it = current_analyses_.constFind(ply);
    if (it == current_analyses_.constEnd()) return;
    const auto &result = it.value();
    const QString variation = result.rawPrincipalVariation.isEmpty()
        ? result.bestMove : result.rawPrincipalVariation;
    if (variation.isEmpty() || result.boardBefore.isEmpty()) return;

    EngineVariationDialog dialog(
        result.boardBefore, result.sideToMove, variation,
        QString::fromUtf8(u8"Pikafish 主变招 · 第 %1 步 · 推荐 %2")
            .arg(ply).arg(result.bestNotation), this);
    dialog.exec();
}

void MainWindow::sendCoachQuestion()
{
    const QString question = coach_question_edit_->text().trimmed();
    if (question.isEmpty()) return;
    if (!coach_ || !coach_->isConfigured()) {
        QMessageBox::information(this, QString::fromUtf8(u8"AI 教练未启用"),
                                 QString::fromUtf8(u8"请先在“AI 设置”中配置 DeepSeek API Key。"));
        return;
    }
    if (current_game_id_ < 0 || coach_chat_context_.trimmed().isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8(u8"还没有可追问的棋局证据"),
                                 QString::fromUtf8(u8"请先走一步并等待引擎分析，或完成一局棋。"));
        return;
    }
    active_chat_request_id_ = QStringLiteral("main-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    pending_chat_game_id_ = current_game_id_;
    appendChatBubble(true, question);
    database_.recordChatMessage(active_user_id_, pending_chat_game_id_,
                                pending_chat_ply_, "user", question);
    coach_question_edit_->clear();
    coach_question_button_->setEnabled(false);
    const QString previousHistory = coach_chat_history_;
    coach_chat_history_ += QString::fromUtf8(u8"学习者：%1\n").arg(question);
    coach_->requestChat(active_chat_request_id_, coach_chat_context_,
                        previousHistory, question);
}

void MainWindow::handleChatReply(const QString &requestId, const QString &answer,
                                 const QString &errorMessage)
{
    if (requestId != active_chat_request_id_) return;
    active_chat_request_id_.clear();
    coach_question_button_->setEnabled(true);
    if (!errorMessage.isEmpty()) {
        appendChatBubble(false, errorMessage, true);
        return;
    }
    appendChatBubble(false, answer);
    database_.recordChatMessage(active_user_id_, pending_chat_game_id_,
                                pending_chat_ply_, "assistant", answer);
    coach_chat_history_ += QString::fromUtf8(u8"AI 教练：%1\n").arg(answer);
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
    current_game_end_reason_ = QStringLiteral("normal");
    analysis_browser_->clear();
    current_analyses_.clear();
    clearAdviceCards();
    coach_chat_context_.clear();
    coach_chat_history_.clear();
    active_chat_request_id_.clear();
    coach_question_button_->setEnabled(true);
    appendAdviceCard(QString::fromUtf8(u8"新对局"),
                     QString::fromUtf8(u8"AI 只解释有训练价值的决策，不打断优秀着法。"),
                     {}, {}, QStringLiteral("notice"));
    current_game_id_ = database_.startGame(active_user_id_, &error);
    if (current_game_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法创建新对局"), error);
    }
    refreshStats();
}

void MainWindow::resignGame()
{
    if (current_game_id_ < 0
        || board_widget_->game().result() != XiangqiGame::GameResult::Ongoing) {
        QMessageBox::information(this, QString::fromUtf8(u8"无法认输"),
                                 QString::fromUtf8(u8"当前没有正在进行的有效对局。"));
        return;
    }
    const auto answer = QMessageBox::question(
        this, QString::fromUtf8(u8"确认认输"),
        QString::fromUtf8(u8"认输后本局将记为黑方获胜，并保存到你的个人数据库。确定继续吗？"));
    if (answer != QMessageBox::Yes) {
        return;
    }
    current_game_end_reason_ = QStringLiteral("resignation");
    if (!board_widget_->resign(XiangqiGame::Side::Red)) {
        current_game_end_reason_ = QStringLiteral("normal");
        return;
    }
    QMessageBox::information(this, QString::fromUtf8(u8"本局结束"),
                             QString::fromUtf8(u8"红方认输，黑方获胜。本局数据已经保存。"));
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
    bool evidenceSaved = true;
    if (current_game_id_ >= 0) {
        evidenceSaved = database_.recordUndoEvent(current_game_id_, lastKeptPly,
                                                   undone, &error);
        if (!evidenceSaved) {
            QMessageBox::warning(
                this, QString::fromUtf8(u8"悔棋学习记录错误"),
                QString::fromUtf8(u8"棋盘已经退回，但无法保存本次悔棋证据：") + error);
        }
        error.clear();
    }
    if (current_game_id_ >= 0 &&
        !database_.truncateGame(current_game_id_, lastKeptPly, &error)) {
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"悔棋记录错误"),
                             QString::fromUtf8(u8"棋盘已经退回，但数据库同步失败：") + error);
    }

    markUndoneAdviceCards(lastKeptPly);
    engine_status_label_->setText(
        evidenceSaved
            ? QString::fromUtf8(u8"已回退到第 %1 步；撤销分支的原分析已保留").arg(lastKeptPly)
            : QString::fromUtf8(u8"已回退到第 %1 步；悔棋证据保存失败").arg(lastKeptPly));
    refreshStats();
}

void MainWindow::startPersonalTraining()
{
    TrainingDialog dialog(&database_, active_user_id_, this);
    connect(&dialog, &TrainingDialog::coachQuestionAsked,
            coach_, &DeepSeekCoach::requestChat);
    connect(coach_, &DeepSeekCoach::chatReplyReady,
            &dialog, &TrainingDialog::receiveCoachReply);
    connect(&dialog, &TrainingDialog::generatedExerciseRequested, &dialog,
            [this, &dialog](const QString &requestId) {
        if (!coach_ || !coach_->isConfigured()) {
            dialog.generatedExerciseFailed(
                requestId, QString::fromUtf8(u8"请先在 AI 设置中配置 DeepSeek API Key。"));
            return;
        }
        coach_->requestGeneratedExercise(requestId,
                                         database_.trainingGenerationContext(active_user_id_));
    });
    connect(coach_, &DeepSeekCoach::exerciseDraftReady, &dialog,
            [this, &dialog](const DeepSeekCoach::ExerciseDraft &draft,
                            const QString &errorMessage) {
        if (!errorMessage.isEmpty()) {
            dialog.generatedExerciseFailed(draft.requestId, errorMessage);
            return;
        }
        QString validationError;
        if (!analyzer_ || !analyzer_->analyzeTrainingPosition(
                              draft.requestId, draft.board, XiangqiGame::Side::Red,
                              &validationError)) {
            dialog.generatedExerciseFailed(draft.requestId, validationError);
            return;
        }
        pending_exercise_drafts_.insert(draft.requestId, draft);
    });
    connect(analyzer_, &PikafishAnalyzer::trainingPositionAnalyzed, &dialog,
            [this, &dialog](const PikafishAnalyzer::PositionAnalysis &analysis,
                            const QString &validationError) {
        const auto it = pending_exercise_drafts_.find(analysis.requestId);
        if (it == pending_exercise_drafts_.end()) return;
        const DeepSeekCoach::ExerciseDraft draft = it.value();
        pending_exercise_drafts_.erase(it);
        if (!validationError.isEmpty()) {
            dialog.generatedExerciseFailed(analysis.requestId, validationError);
            return;
        }
        const QString pv = PikafishAnalyzer::toChinesePrincipalVariation(
            analysis.board.toStdString(), analysis.sideToMove,
            analysis.rawPrincipalVariation);
        QString storageError;
        if (database_.storeGeneratedTrainingPosition(
                active_user_id_, analysis.board, analysis.bestMove, pv, analysis.score,
                draft.theme, draft.diagnosisTag, draft.learningGoal, draft.hint,
                &storageError) < 0) {
            dialog.generatedExerciseFailed(analysis.requestId, storageError);
            return;
        }
        dialog.generatedExerciseReady(analysis.requestId);
    });
    connect(&dialog, &TrainingDialog::wrongLineRequested, &dialog,
            [this, &dialog](const QString &requestId, const QString &boardBefore,
                            const QString &boardAfterWrong, const QString &wrongMove) {
        if (!analyzer_) {
            dialog.wrongLineAnalysisFailed(requestId, QString::fromUtf8(u8"Pikafish 未初始化。"));
            return;
        }
        pending_training_lines_.insert(requestId, PendingTrainingLine{boardBefore, wrongMove});
        QString error;
        if (!analyzer_->analyzeTrainingPosition(requestId, boardAfterWrong,
                                                XiangqiGame::Side::Black, &error)) {
            pending_training_lines_.remove(requestId);
            dialog.wrongLineAnalysisFailed(requestId, error);
        }
    });
    connect(analyzer_, &PikafishAnalyzer::trainingPositionAnalyzed, &dialog,
            [this, &dialog](const PikafishAnalyzer::PositionAnalysis &analysis,
                            const QString &analysisError) {
        const auto it = pending_training_lines_.find(analysis.requestId);
        if (it == pending_training_lines_.end()) return;
        const PendingTrainingLine line = it.value();
        pending_training_lines_.erase(it);
        if (!analysisError.isEmpty() || analysis.rawPrincipalVariation.isEmpty()) {
            dialog.wrongLineAnalysisFailed(
                analysis.requestId,
                analysisError.isEmpty()
                    ? QString::fromUtf8(u8"引擎没有返回可推演的惩罚线。")
                    : analysisError);
            return;
        }
        const QString variation = line.wrongMove + QStringLiteral(" ")
            + analysis.rawPrincipalVariation;
        EngineVariationDialog variationDialog(
            line.boardBefore, XiangqiGame::Side::Red, variation,
            QString::fromUtf8(u8"错误着后的引擎惩罚线"), &dialog);
        dialog.wrongLineAnalysisFinished(analysis.requestId);
        variationDialog.exec();
    });
    dialog.exec();
    refreshStats();
}

void MainWindow::openGameReview()
{
    GameReviewDialog dialog(&database_, active_user_id_, this);
    connect(&dialog, &GameReviewDialog::coachQuestionAsked,
            coach_, &DeepSeekCoach::requestChat);
    connect(coach_, &DeepSeekCoach::chatReplyReady,
            &dialog, &GameReviewDialog::receiveChatReply);
    dialog.exec();
    for (qint64 gameId : dialog.deletedGameIds()) {
        pending_game_reviews_.remove(gameId);
        if (gameId == current_game_id_) current_game_id_ = -1;
    }
    refreshStats();
    showMilestoneReportIfNeeded();
}

void MainWindow::favoriteScore()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8(u8"收藏棋谱"), QString(),
        QString::fromUtf8(u8"棋谱文件 (*.txt *.gif);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    ChessScoreImporter::ParsedScore parsed;
    QString error;
    if (!ChessScoreImporter::parseFile(path, &parsed, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"棋谱无法收藏"), error);
        return;
    }
    GameDatabase::FavoriteScore favorite;
    favorite.userId = active_user_id_;
    favorite.title = parsed.title;
    favorite.sourceFile = parsed.sourceFile;
    favorite.sourceFormat = parsed.sourceFormat;
    favorite.initialBoard = parsed.initialBoard;
    favorite.sideToMove = parsed.sideToMove;
    favorite.moves = parsed.moves.join(QLatin1Char(' '));
    favorite.rawContent = parsed.rawContent;
    const qint64 id = database_.saveFavoriteScore(favorite, &error);
    if (id < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"棋谱收藏失败"), error);
        return;
    }

    const QString detail = parsed.warning.isEmpty()
        ? QString::fromUtf8(u8"已保存为可复盘棋谱，着法数：") + QString::number(parsed.moves.size())
        : parsed.warning;
    engine_status_label_->setText(QString::fromUtf8(u8"棋谱已收藏：") + parsed.title + QStringLiteral(" · ") + detail);
    if (parsed.initialBoard.isEmpty() || parsed.moves.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8(u8"棋谱已收藏"), detail);
        return;
    }

    EngineVariationDialog dialog(
        parsed.initialBoard,
        parsed.sideToMove.compare(QStringLiteral("b"), Qt::CaseInsensitive) == 0
            ? XiangqiGame::Side::Black : XiangqiGame::Side::Red,
        parsed.moves.join(QLatin1Char(' ')),
        QString::fromUtf8(u8"已收藏棋谱 · ") + parsed.title, this);
    dialog.exec();
}

void MainWindow::openFavoriteScores()
{
    const QVector<GameDatabase::FavoriteScore> scores = database_.favoriteScores(active_user_id_);
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8(u8"收藏夹"));
    dialog.resize(620, 420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const auto &score : scores) {
        const QString moves = score.moves.isEmpty()
            ? QString::fromUtf8(u8"仅局面")
            : QString::fromUtf8(u8"着法 ") + QString::number(score.moves.split(' ', Qt::SkipEmptyParts).size()) + QString::fromUtf8(u8" 步");
        auto *item = new QListWidgetItem(
            score.title + QStringLiteral(" · ") + score.sourceFormat + QStringLiteral(" · ") + moves, list);
        item->setData(Qt::UserRole, score.id);
    }
    layout->addWidget(list, 1);
    auto *closeButton = new QPushButton(QString::fromUtf8(u8"关闭"), &dialog);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, &dialog,
            [this, &dialog, scores](QListWidgetItem *item) {
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        for (const auto &score : scores) {
            if (score.id != id || score.initialBoard.isEmpty() || score.moves.isEmpty()) continue;
            EngineVariationDialog variation(
                score.initialBoard,
                score.sideToMove.compare(QStringLiteral("b"), Qt::CaseInsensitive) == 0
                    ? XiangqiGame::Side::Black : XiangqiGame::Side::Red,
                score.moves, score.title, &dialog);
            variation.exec();
            return;
        }
        QMessageBox::information(&dialog, QString::fromUtf8(u8"无法复盘"),
                                 QString::fromUtf8(u8"这个收藏只保存了局面或原始文件，没有完整着法。"));
    });
    dialog.exec();
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
    current_game_end_reason_ = QStringLiteral("normal");
    analysis_browser_->clear();
    current_analyses_.clear();
    clearAdviceCards();
    appendAdviceCard(QString::fromUtf8(u8"用户已切换"),
                     QString::fromUtf8(u8"当前对局和训练数据只属于所选用户。"),
                     {}, {}, QStringLiteral("notice"));
    coach_chat_context_.clear();
    coach_chat_history_.clear();
    active_chat_request_id_.clear();
    coach_question_button_->setEnabled(true);
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
    QString personalizationError;
    if (!database_.rebuildPersonalization(active_user_id_, &personalizationError)
        && engine_status_label_) {
        engine_status_label_->setText(QString::fromUtf8(u8"更新动态画像失败：")
                                      + personalizationError);
    }
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
    auto statusText = [](const QString &status) {
        if (status == "isolated") return QString::fromUtf8(u8"偶发现象");
        if (status == "needs_verification") return QString::fromUtf8(u8"待验证问题");
        if (status == "stable_weakness") return QString::fromUtf8(u8"较稳定弱点");
        return QString::fromUtf8(u8"证据不足");
    };
    auto trendText = [](const QString &trend) {
        if (trend == "improving") return QString::fromUtf8(u8"正在改善");
        if (trend == "worsening") return QString::fromUtf8(u8"近期增加");
        return QString::fromUtf8(u8"基本稳定");
    };
    QString dimensionHtml;
    const auto dimensions = database_.profileDimensions(active_user_id_);
    for (const auto &dimension : dimensions) {
        if (dimension.evidenceCount == 0) continue;
        dimensionHtml += QString::fromUtf8(
            u8"<tr><td><b>%1</b></td><td>%2</td><td>%3</td><td>%4</td>"
            u8"<td>%5 条 / %6 盘</td></tr>")
            .arg(dimension.title.toHtmlEscaped()).arg(dimension.score)
            .arg(dimension.confidence, 0, 'f', 2)
            .arg(statusText(dimension.status) + QString::fromUtf8(u8" · ")
                 + trendText(dimension.trend))
            .arg(dimension.evidenceCount).arg(dimension.gameCount);
    }
    if (dimensionHtml.isEmpty()) {
        dimensionHtml = QString::fromUtf8(u8"<tr><td colspan='5'>证据不足，继续完成有效对局后更新。</td></tr>");
    }
    const auto plan = database_.currentTrainingPlan(active_user_id_);
    QString planHtml = QString::fromUtf8(u8"尚未形成阶段训练计划。");
    if (plan.id >= 0) {
        QString items;
        for (const auto &item : plan.items) {
            items += QString::fromUtf8(u8"<li><b>%1</b>（%2 / %3）<br><small>%4</small></li>")
                .arg(item.title.toHtmlEscaped()).arg(item.completedCount)
                .arg(item.targetCount).arg(item.reason.toHtmlEscaped());
        }
        planHtml = QString::fromUtf8(
            u8"<b>诊断假设：</b>%1（置信度 %2）<br><b>验证标准：</b>%3<ul>%4</ul>")
            .arg(plan.hypothesis.toHtmlEscaped()).arg(plan.confidence, 0, 'f', 2)
            .arg(plan.successMetric.toHtmlEscaped(), items);
    }
    const QString userName = user_combo_ ? user_combo_->currentText() : QString();
    profile_dashboard_->setProfileData(
        userName, profile, stats, training,
        database_.recentGamePerformance(active_user_id_, 10));
    stats_browser_->setHtml(QString::fromUtf8(
        u8"<h3>%1 的训练说明</h3>"
        u8"<b>当前训练重点</b><br>%2<br><br>"
        u8"个人错题：%3　今日到期：%4　AI讲解：%5　悔棋证据：%6<hr>"
        u8"<h3>动态能力画像</h3><table cellspacing='6'>"
        u8"<tr><th>维度</th><th>分数</th><th>置信度</th><th>状态/趋势</th><th>证据</th></tr>%7</table><hr>"
        u8"<h3>当前阶段训练计划</h3>%8<hr>"
        u8"<b>最近阶段总结</b><br>%9")
        .arg(userName.toHtmlEscaped(),
             profile.mainWeakness.toHtmlEscaped())
        .arg(training.positions)
        .arg(training.due)
        .arg(stats.coachedMoves)
        .arg(stats.undoEvents)
        .arg(dimensionHtml, planHtml, latestReport));
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
