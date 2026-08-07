#include "game_review_dialog.h"

#include "pikafish_analyzer.h"
#include "review_branch_dialog.h"
#include "xiangqi_board_widget.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QVariant>
#include <QUuid>

namespace {
QString resultText(const QString &result, const QString &reason)
{
    if (reason == "resignation") return QString::fromUtf8(u8"红方认输");
    if (result == "red_wins") return QString::fromUtf8(u8"红胜");
    if (result == "black_wins") return QString::fromUtf8(u8"黑胜");
    if (result == "draw") return QString::fromUtf8(u8"和棋");
    return result;
}

QString categoryText(const QString &category)
{
    if (category == "excellent") return QString::fromUtf8(u8"优秀");
    if (category == "inaccuracy") return QString::fromUtf8(u8"轻微失误");
    if (category == "mistake") return QString::fromUtf8(u8"明显失误");
    if (category == "blunder") return QString::fromUtf8(u8"严重失误");
    return QString::fromUtf8(u8"尚未分析");
}

QString htmlText(QString text)
{
    return text.toHtmlEscaped().replace("\n", "<br>");
}
}

GameReviewDialog::GameReviewDialog(GameDatabase *database, qint64 userId,
                                   QWidget *parent)
    : QDialog(parent), database_(database), user_id_(userId)
{
    setWindowTitle(QString::fromUtf8(u8"历史对局复盘"));
    setMinimumSize(1080, 720);
    resize(1280, 800);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);
    auto *heading = new QLabel(QString::fromUtf8(u8"时间轴复盘"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(17);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    root->addWidget(heading);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    game_list_ = new QListWidget(splitter);
    game_list_->setMinimumWidth(235);
    game_list_->setMaximumWidth(310);

    auto *content = new QWidget(splitter);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 0, 0, 0);
    game_title_ = new QLabel(content);
    game_title_->setWordWrap(true);
    game_title_->setStyleSheet("font-size:14px;font-weight:600;padding:7px;");
    contentLayout->addWidget(game_title_);

    auto *reviewSplitter = new QSplitter(Qt::Horizontal, content);
    reviewSplitter->setChildrenCollapsible(false);
    board_ = new XiangqiBoardWidget(reviewSplitter, false);
    board_->setMinimumSize(500, 570);
    detail_tabs_ = new QTabWidget(reviewSplitter);
    detail_tabs_->setMinimumWidth(360);
    move_detail_ = new QTextBrowser(detail_tabs_);
    move_detail_->setStyleSheet("font-size:14px;line-height:1.5;padding:12px;");
    whole_review_ = new QTextBrowser(detail_tabs_);
    whole_review_->setStyleSheet("font-size:14px;line-height:1.5;padding:12px;");
    undo_review_ = new QTextBrowser(detail_tabs_);
    undo_review_->setStyleSheet("font-size:14px;line-height:1.5;padding:12px;");
    auto *chatTab = new QWidget(detail_tabs_);
    auto *chatLayout = new QVBoxLayout(chatTab);
    chatLayout->setContentsMargins(8, 8, 8, 8);
    chat_browser_ = new QTextBrowser(chatTab);
    chat_edit_ = new QLineEdit(chatTab);
    chat_edit_->setPlaceholderText(QString::fromUtf8(
        u8"针对当前着法或整盘复盘追问，例如：为什么推荐着法更好？"));
    chat_button_ = new QPushButton(QString::fromUtf8(u8"发送"), chatTab);
    auto *chatRow = new QHBoxLayout;
    chatRow->addWidget(chat_edit_, 1);
    chatRow->addWidget(chat_button_);
    chatLayout->addWidget(chat_browser_, 1);
    chatLayout->addLayout(chatRow);
    detail_tabs_->addTab(move_detail_, QString::fromUtf8(u8"当前着法"));
    detail_tabs_->addTab(whole_review_, QString::fromUtf8(u8"整盘建议"));
    detail_tabs_->addTab(undo_review_, QString::fromUtf8(u8"悔棋记录"));
    detail_tabs_->addTab(chatTab, QString::fromUtf8(u8"追问教练"));
    reviewSplitter->addWidget(board_);
    reviewSplitter->addWidget(detail_tabs_);
    reviewSplitter->setSizes({620, 430});
    contentLayout->addWidget(reviewSplitter, 1);

    auto *navigation = new QHBoxLayout;
    first_button_ = new QPushButton(QString::fromUtf8(u8"|< 首局面"), content);
    previous_button_ = new QPushButton(QString::fromUtf8(u8"< 上一步"), content);
    next_button_ = new QPushButton(QString::fromUtf8(u8"下一步 >"), content);
    last_button_ = new QPushButton(QString::fromUtf8(u8"末局面 >|"), content);
    branch_button_ = new QPushButton(QString::fromUtf8(u8"尝试替代着法"), content);
    branch_button_->setObjectName("branchButton");
    timeline_ = new QSlider(Qt::Horizontal, content);
    position_label_ = new QLabel(content);
    position_label_->setMinimumWidth(90);
    position_label_->setAlignment(Qt::AlignCenter);
    navigation->addWidget(first_button_);
    navigation->addWidget(previous_button_);
    navigation->addWidget(timeline_, 1);
    navigation->addWidget(position_label_);
    navigation->addWidget(next_button_);
    navigation->addWidget(last_button_);
    navigation->addWidget(branch_button_);
    contentLayout->addLayout(navigation);

    splitter->addWidget(game_list_);
    splitter->addWidget(content);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    delete_button_ = new QPushButton(QString::fromUtf8(u8"删除所选对局"), buttons);
    delete_button_->setObjectName("deleteGameButton");
    buttons->addButton(delete_button_, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(delete_button_, &QPushButton::clicked,
            this, &GameReviewDialog::deleteSelectedGame);
    root->addWidget(buttons);

    connect(game_list_, &QListWidget::currentRowChanged,
            this, [this] { loadSelectedGame(); });
    connect(timeline_, &QSlider::valueChanged,
            this, [this](int value) { showPosition(value); });
    connect(first_button_, &QPushButton::clicked, this, [this] { showPosition(0); });
    connect(previous_button_, &QPushButton::clicked,
            this, [this] { showPosition(position_index_ - 1); });
    connect(next_button_, &QPushButton::clicked,
            this, [this] { showPosition(position_index_ + 1); });
    connect(last_button_, &QPushButton::clicked,
            this, [this] { showPosition(static_cast<int>(moves_.size())); });
    connect(branch_button_, &QPushButton::clicked,
            this, &GameReviewDialog::openBranch);
    connect(chat_button_, &QPushButton::clicked,
            this, &GameReviewDialog::sendChatQuestion);
    connect(chat_edit_, &QLineEdit::returnPressed,
            this, &GameReviewDialog::sendChatQuestion);

    setStyleSheet(QString::fromUtf8(R"(
        QDialog { background:#f4f1e9; color:#29251f; }
        QListWidget, QTextBrowser, QTabWidget::pane {
            background:#fffdf8; border:1px solid #ddd5c6; border-radius:7px;
        }
        QListWidget::item { padding:10px 8px; border-bottom:1px solid #eee8dd; }
        QListWidget::item:selected { background:#eadfd2; color:#7d3025; }
        QPushButton { min-height:32px; padding:0 12px; border:1px solid #cfc6b7;
                      border-radius:6px; background:#fffdf8; }
        QPushButton:hover { background:#efe7da; }
        QPushButton#deleteGameButton { color:#9b342b; border-color:#d9a8a1; }
        QPushButton#deleteGameButton:hover { background:#fbe8e5; }
        QPushButton#branchButton { color:#315f54; border-color:#9bb8ac; }
        QPushButton#branchButton:hover { background:#e5f0eb; }
    )"));
    loadGames();
}

QVector<qint64> GameReviewDialog::deletedGameIds() const
{
    return deleted_game_ids_;
}

void GameReviewDialog::loadGames()
{
    game_list_->clear();
    if (!database_) return;
    const auto games = database_->completedGames(user_id_);
    for (const auto &game : games) {
        const QString reviewMark = game.hasReview ? QString::fromUtf8(u8" · 已总结") : QString();
        auto *item = new QListWidgetItem(
            QString::fromUtf8(u8"第 %1 局  %2\n%3  · %4 步%5")
                .arg(game.sequenceNumber).arg(resultText(game.result, game.endReason))
                .arg(game.startedAt.left(16)).arg(game.moveCount).arg(reviewMark),
            game_list_);
        item->setData(Qt::UserRole, game.id);
        item->setData(Qt::UserRole + 1, game.sequenceNumber);
    }
    if (game_list_->count() > 0) {
        game_list_->setCurrentRow(0);
    } else {
        game_id_ = -1;
        moves_.clear();
        game_title_->setText(QString::fromUtf8(u8"当前用户还没有可复盘的已完成对局。"));
        move_detail_->setHtml(QString::fromUtf8(u8"<p>完成一盘棋后，对局会出现在这里。</p>"));
        whole_review_->clear();
        board_->newGame();
        updateNavigation();
    }
}

void GameReviewDialog::loadSelectedGame()
{
    auto *item = game_list_->currentItem();
    if (!item || !database_) return;
    game_id_ = item->data(Qt::UserRole).toLongLong();
    moves_ = database_->recordedMoves(game_id_);
    game_title_->setText(item->text().replace('\n', QString::fromUtf8(u8"　")));
    whole_review_->setHtml(reviewHtml(game_id_));
    undo_review_->setHtml(undoHtml(game_id_));
    loadChatHistory();
    timeline_->setRange(0, static_cast<int>(moves_.size()));
    showPosition(0);
}

void GameReviewDialog::showPosition(int index)
{
    if (moves_.isEmpty()) {
        position_index_ = 0;
        move_detail_->setHtml(QString::fromUtf8(u8"<p>这盘棋没有保存的着法。</p>"));
        updateNavigation();
        return;
    }
    position_index_ = std::clamp(index, 0, static_cast<int>(moves_.size()));
    const auto &reference = position_index_ == 0 ? moves_.front() : moves_[position_index_ - 1];
    const QString board = position_index_ == 0 ? moves_.front().boardBefore : reference.boardAfter;
    const XiangqiGame::Side sideToMove = position_index_ == 0
        ? XiangqiGame::Side::Red
        : (reference.side == "red" ? XiangqiGame::Side::Black : XiangqiGame::Side::Red);
    if (position_index_ == 0) {
        board_->loadReviewPosition(board.toStdString(), sideToMove);
        move_detail_->setHtml(QString::fromUtf8(
            u8"<h3>初始局面</h3><p>点击“下一步”或拖动时间轴，查看着法及其评价。</p>"));
    } else {
        board_->loadReviewPosition(board.toStdString(), sideToMove,
                                   reference.fromRow, reference.fromCol,
                                   reference.toRow, reference.toCol);
        move_detail_->setHtml(moveHtml(reference));
    }
    if (timeline_->value() != position_index_) timeline_->setValue(position_index_);
    updateNavigation();
}

void GameReviewDialog::updateNavigation()
{
    const int maximum = static_cast<int>(moves_.size());
    first_button_->setEnabled(position_index_ > 0);
    previous_button_->setEnabled(position_index_ > 0);
    next_button_->setEnabled(position_index_ < maximum);
    last_button_->setEnabled(position_index_ < maximum);
    const bool canBranch = position_index_ >= 0
        && position_index_ < maximum
        && moves_.at(position_index_).side == "red";
    branch_button_->setEnabled(canBranch);
    branch_button_->setToolTip(canBranch
        ? QString::fromUtf8(u8"从当前红方落子前创建临时分支")
        : QString::fromUtf8(u8"请选择红方着法前的局面"));
    timeline_->setEnabled(maximum > 0);
    position_label_->setText(QString::fromUtf8(u8"局面 %1 / %2")
                                 .arg(position_index_).arg(maximum));
}

void GameReviewDialog::openBranch()
{
    if (position_index_ < 0 || position_index_ >= moves_.size()) return;
    const auto &move = moves_.at(position_index_);
    if (move.side != "red") return;
    ReviewBranchDialog dialog(move.boardBefore, this);
    dialog.exec();
}

void GameReviewDialog::deleteSelectedGame()
{
    auto *item = game_list_->currentItem();
    if (!item || !database_) return;
    const qint64 gameId = item->data(Qt::UserRole).toLongLong();
    const int sequenceNumber = item->data(Qt::UserRole + 1).toInt();
    const auto answer = QMessageBox::warning(
        this, QString::fromUtf8(u8"删除对局"),
        QString::fromUtf8(
            u8"确定删除第 %1 局吗？\n\n这会永久删除该局的棋谱、引擎分析、AI 建议、"
            u8"整盘总结以及由该局生成的专项训练记录，并重新计算个人画像。此操作不能撤销。")
            .arg(sequenceNumber),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) return;

    QString error;
    if (!database_->deleteCompletedGame(user_id_, gameId, &error)) {
        QMessageBox::critical(this, QString::fromUtf8(u8"删除失败"), error);
        return;
    }
    deleted_game_ids_.push_back(gameId);
    game_id_ = -1;
    moves_.clear();
    loadGames();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"对局已删除"), error);
    }
}

void GameReviewDialog::sendChatQuestion()
{
    const QString question = chat_edit_->text().trimmed();
    if (question.isEmpty() || game_id_ < 0 || !chat_request_id_.isEmpty()) return;
    const int ply = position_index_ > 0 && position_index_ <= moves_.size()
        ? moves_[position_index_ - 1].ply : 0;
    chat_request_id_ = QStringLiteral("review-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    chat_request_game_id_ = game_id_;
    chat_request_ply_ = ply;
    database_->recordChatMessage(user_id_, game_id_, ply, "user", question);
    const QString previousHistory = chat_history_;
    chat_history_ += QString::fromUtf8(u8"学习者：%1\n").arg(question);
    chat_browser_->append(QString::fromUtf8(
        u8"<div style='text-align:right;margin:8px'><b>你：</b>%1</div>")
        .arg(question.toHtmlEscaped()));
    chat_edit_->clear();
    chat_button_->setEnabled(false);
    emit coachQuestionAsked(chat_request_id_, chatEvidenceContext(),
                            previousHistory, question);
}

void GameReviewDialog::receiveChatReply(const QString &requestId,
                                        const QString &answer,
                                        const QString &errorMessage)
{
    if (requestId != chat_request_id_) return;
    chat_request_id_.clear();
    chat_button_->setEnabled(true);
    if (!errorMessage.isEmpty()) {
        chat_browser_->append(QString::fromUtf8(
            u8"<p style='color:#9b342b'><b>回答失败：</b>%1</p>")
            .arg(errorMessage.toHtmlEscaped()));
        return;
    }
    database_->recordChatMessage(user_id_, chat_request_game_id_,
                                 chat_request_ply_, "assistant", answer);
    if (game_id_ != chat_request_game_id_) return;
    chat_history_ += QString::fromUtf8(u8"AI 教练：%1\n").arg(answer);
    chat_browser_->append(QString::fromUtf8(
        u8"<div style='border-left:4px solid #5b4bb7;padding:8px;margin:8px 0'>"
        u8"<b>AI 教练：</b><br>%1</div>")
        .arg(answer.toHtmlEscaped().replace("\n", "<br>")));
}

void GameReviewDialog::loadChatHistory()
{
    chat_browser_->clear();
    chat_history_.clear();
    if (!database_ || game_id_ < 0) return;
    const auto messages = database_->chatMessages(user_id_, game_id_);
    for (const auto &message : messages) {
        if (message.role == "assistant") {
            chat_browser_->append(QString::fromUtf8(
                u8"<div style='border-left:4px solid #5b4bb7;padding:8px;margin:8px 0'>"
                u8"<b>AI 教练：</b><br>%1</div>")
                .arg(message.content.toHtmlEscaped().replace("\n", "<br>")));
            chat_history_ += QString::fromUtf8(u8"AI 教练：%1\n").arg(message.content);
        } else {
            chat_browser_->append(QString::fromUtf8(
                u8"<div style='text-align:right;margin:8px'><b>你：</b>%1</div>")
                .arg(message.content.toHtmlEscaped()));
            chat_history_ += QString::fromUtf8(u8"学习者：%1\n").arg(message.content);
        }
    }
    if (messages.isEmpty()) {
        chat_browser_->setHtml(QString::fromUtf8(
            u8"<p style='color:#777'>选择某一步后，可以针对该局面和引擎证据追问 AI 教练。对话会保存在这盘棋中。</p>"));
    }
}

QString GameReviewDialog::moveHtml(const GameDatabase::RecordedMove &move) const
{
    const auto side = move.side == "red" ? XiangqiGame::Side::Red : XiangqiGame::Side::Black;
    const QString notation = PikafishAnalyzer::toChineseNotation(
        move.boardBefore.toStdString(), side, move.actualMove);
    QString html = QString::fromUtf8(
        u8"<h2>第 %1 步 · %2</h2><p><b>实际着法：</b>%3 <code>%4</code><br>"
        u8"<b>思考时间：</b>%5 秒</p>")
        .arg(move.ply).arg(move.side == "red" ? QString::fromUtf8(u8"红方")
                                               : QString::fromUtf8(u8"黑方"))
        .arg(notation.toHtmlEscaped(), move.actualMove.toHtmlEscaped())
        .arg(move.thinkingTimeMs / 1000.0, 0, 'f', 1);
    if (!move.hasAnalysis) {
        return html + QString::fromUtf8(u8"<p style='color:#777'>这一着没有保存引擎分析。</p>");
    }
    const QString bestNotation = PikafishAnalyzer::toChineseNotation(
        move.boardBefore.toStdString(), side, move.bestMove);
    html += QString::fromUtf8(
        u8"<div style='background:#f7f1e5;border-radius:7px;padding:10px'>"
        u8"<b>引擎评价：</b>%1<br><b>推荐着法：</b>%2<br>"
        u8"<b>局面评价下降：</b>%3 分</div>")
        .arg(categoryText(move.category), bestNotation.toHtmlEscaped())
        .arg(move.scoreLoss);
    if (!move.diagnosis.isEmpty()) {
        html += QString::fromUtf8(
            u8"<h3>AI 教练意见</h3><p><b>诊断：</b>%1<br><b>关键变化：</b>%2<br>"
            u8"<b>推荐着的目的：</b>%3<br><b>实战判定标准：</b>%4</p>")
            .arg(htmlText(move.diagnosis), htmlText(move.evidence),
                 htmlText(move.trainingTask), htmlText(move.reflectionQuestion));
    }
    return html;
}

QString GameReviewDialog::reviewHtml(qint64 gameId) const
{
    const auto review = database_->gameReview(gameId);
    if (review.gameId < 0) {
        return QString::fromUtf8(u8"<p>这盘棋还没有整盘总结。</p>");
    }
    return QString::fromUtf8(
        u8"<h2>整盘建议</h2><b>总体评价</b><p>%1</p>"
        u8"<b>关键转折点</b><p>%2</p><b>做得好的地方</b><p>%3</p>"
        u8"<b>重复思考模式</b><p>%4</p><b>训练计划</b><p>%5</p>"
        u8"<b>复盘问题</b><p>%6</p>")
        .arg(htmlText(review.overview), htmlText(review.turningPoints),
             htmlText(review.strengths), htmlText(review.recurringPattern),
             htmlText(review.trainingPlan), htmlText(review.reflectionQuestion));
}

QString GameReviewDialog::undoHtml(qint64 gameId) const
{
    const auto events = database_->gameUndoEvents(gameId);
    if (events.isEmpty()) {
        return QString::fromUtf8(u8"<p>这盘棋没有悔棋记录。</p>");
    }
    QString html = QString::fromUtf8(
        u8"<h2>悔棋学习记录</h2><p style='color:#6d6256'>"
        u8"这些着法即使从正式棋谱中撤销，也会作为独立学习证据永久保留。</p>");
    for (const auto &event : events) {
        const QString actualNotation = PikafishAnalyzer::toChineseNotation(
            event.boardBefore.toStdString(), XiangqiGame::Side::Red, event.actualMove);
        const QString bestNotation = event.bestMove.isEmpty() ? QString() :
            PikafishAnalyzer::toChineseNotation(
                event.boardBefore.toStdString(), XiangqiGame::Side::Red, event.bestMove);
        html += QString::fromUtf8(
            u8"<div style='border-left:4px solid #b47b32;background:#fff8e9;"
            u8"padding:10px 12px;margin:10px 0'><h3>第 %1 步后悔棋</h3>"
            u8"<b>撤销的着法：</b>%2 <code>%3</code><br>")
            .arg(event.redMovePly)
            .arg(actualNotation.toHtmlEscaped(), event.actualMove.toHtmlEscaped());
        if (event.hadAnalysis) {
            html += QString::fromUtf8(
                u8"<b>引擎评价：</b>%1，损失 %2<br>"
                u8"<b>推荐着法：</b>%3<br>")
                .arg(categoryText(event.category)).arg(event.scoreLoss)
                .arg(bestNotation.toHtmlEscaped());
        } else {
            html += QString::fromUtf8(
                u8"<span style='color:#777'>引擎分析尚未完成；如果结果稍后返回，系统会自动回填到这里。</span><br>");
        }
        if (!event.diagnosis.isEmpty()) {
            html += QString::fromUtf8(
                u8"<b>AI 教练诊断：</b>%1<br><b>关键变化：</b>%2<br>"
                u8"<b>推荐着的目的：</b>%3<br><b>实战判定标准：</b>%4")
                .arg(htmlText(event.diagnosis), htmlText(event.evidence),
                     htmlText(event.trainingTask), htmlText(event.reflectionQuestion));
        }
        html += "</div>";
    }
    return html;
}

QString GameReviewDialog::chatEvidenceContext() const
{
    QString context = QString::fromUtf8(u8"对局数据库 ID：%1\n").arg(game_id_);
    if (position_index_ > 0 && position_index_ <= moves_.size()) {
        const auto &move = moves_[position_index_ - 1];
        context += QString::fromUtf8(
            u8"当前查看第 %1 步，实际着法 %2，推荐着法 %3，评分 %4→%5，"
            u8"损失 %6，等级 %7，推荐变化 %8，走棋前局面 %9。\n")
            .arg(move.ply).arg(move.actualMove, move.bestMove)
            .arg(move.bestScore).arg(move.actualScore).arg(move.scoreLoss)
            .arg(move.category, move.principalVariation, move.boardBefore);
    } else {
        context += QString::fromUtf8(u8"当前查看整盘初始局面。\n");
    }
    GameDatabase::GameReviewContext reviewContext;
    if (database_->buildGameReviewContext(game_id_, &reviewContext)) {
        context += QString::fromUtf8(
            u8"关键转折点：\n%1\n悔棋证据：\n%2\n完整棋谱：\n%3\n")
            .arg(reviewContext.keyMoments, reviewContext.undoSummary,
                 reviewContext.moveTranscript);
    }
    const auto review = database_->gameReview(game_id_);
    if (review.gameId >= 0) {
        context += QString::fromUtf8(u8"已有整盘复盘：\n%1\n%2\n%3\n%4")
            .arg(review.overview, review.turningPoints,
                 review.recurringPattern, review.trainingPlan);
    }
    return context;
}
