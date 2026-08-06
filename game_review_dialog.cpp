#include "game_review_dialog.h"

#include "pikafish_analyzer.h"
#include "xiangqi_board_widget.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QVariant>

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
    detail_tabs_->addTab(move_detail_, QString::fromUtf8(u8"当前着法"));
    detail_tabs_->addTab(whole_review_, QString::fromUtf8(u8"整盘建议"));
    reviewSplitter->addWidget(board_);
    reviewSplitter->addWidget(detail_tabs_);
    reviewSplitter->setSizes({620, 430});
    contentLayout->addWidget(reviewSplitter, 1);

    auto *navigation = new QHBoxLayout;
    first_button_ = new QPushButton(QString::fromUtf8(u8"|< 首局面"), content);
    previous_button_ = new QPushButton(QString::fromUtf8(u8"< 上一步"), content);
    next_button_ = new QPushButton(QString::fromUtf8(u8"下一步 >"), content);
    last_button_ = new QPushButton(QString::fromUtf8(u8"末局面 >|"), content);
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
    contentLayout->addLayout(navigation);

    splitter->addWidget(game_list_);
    splitter->addWidget(content);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
    )"));
    loadGames();
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
                .arg(game.id).arg(resultText(game.result, game.endReason))
                .arg(game.startedAt.left(16)).arg(game.moveCount).arg(reviewMark),
            game_list_);
        item->setData(Qt::UserRole, game.id);
    }
    if (game_list_->count() > 0) {
        game_list_->setCurrentRow(0);
    } else {
        game_title_->setText(QString::fromUtf8(u8"当前用户还没有可复盘的已完成对局。"));
        move_detail_->setHtml(QString::fromUtf8(u8"<p>完成一盘棋后，对局会出现在这里。</p>"));
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
    timeline_->setEnabled(maximum > 0);
    position_label_->setText(QString::fromUtf8(u8"局面 %1 / %2")
                                 .arg(position_index_).arg(maximum));
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
    const QString pv = PikafishAnalyzer::toChinesePrincipalVariation(
        move.boardBefore.toStdString(), side, move.principalVariation);
    html += QString::fromUtf8(
        u8"<div style='background:#f7f1e5;border-radius:7px;padding:10px'>"
        u8"<b>引擎评价：</b>%1<br><b>推荐着法：</b>%2 <code>%3</code><br>"
        u8"<b>评分变化：</b>%4 → %5　<b>损失：</b>%6<br>"
        u8"<b>推荐变化：</b>%7</div>")
        .arg(categoryText(move.category), bestNotation.toHtmlEscaped(),
             move.bestMove.toHtmlEscaped())
        .arg(move.bestScore).arg(move.actualScore).arg(move.scoreLoss)
        .arg(pv.toHtmlEscaped());
    if (!move.diagnosis.isEmpty()) {
        html += QString::fromUtf8(
            u8"<h3>AI 教练意见</h3><p><b>诊断：</b>%1<br><b>依据：</b>%2<br>"
            u8"<b>训练任务：</b>%3<br><b>复盘问题：</b>%4</p>")
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
