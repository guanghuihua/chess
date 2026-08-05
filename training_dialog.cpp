#include "training_dialog.h"

#include "xiangqi_board_widget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

TrainingDialog::TrainingDialog(GameDatabase *database, qint64 userId, QWidget *parent)
    : QDialog(parent)
    , database_(database)
    , user_id_(userId)
{
    setWindowTitle(QString::fromUtf8(u8"我的专项训练"));
    setMinimumSize(1040, 720);
    resize(1120, 760);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(14);

    auto *boardCard = new QFrame(this);
    boardCard->setObjectName("trainingCard");
    auto *boardLayout = new QVBoxLayout(boardCard);
    boardLayout->setContentsMargins(12, 12, 12, 12);
    board_ = new XiangqiBoardWidget(boardCard, false);
    boardLayout->addWidget(board_);
    root->addWidget(boardCard, 3);

    auto *panel = new QFrame(this);
    panel->setObjectName("trainingCard");
    panel->setMinimumWidth(340);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 18, 18, 18);
    panelLayout->setSpacing(12);

    auto *title = new QLabel(QString::fromUtf8(u8"历史错题再训练"), panel);
    title->setObjectName("trainingTitle");
    panelLayout->addWidget(title);

    progress_label_ = new QLabel(panel);
    progress_label_->setObjectName("mutedText");
    panelLayout->addWidget(progress_label_);

    theme_label_ = new QLabel(panel);
    theme_label_->setObjectName("themeBadge");
    theme_label_->setWordWrap(true);
    panelLayout->addWidget(theme_label_);

    source_label_ = new QLabel(panel);
    source_label_->setObjectName("mutedText");
    source_label_->setWordWrap(true);
    panelLayout->addWidget(source_label_);

    auto *instruction = new QLabel(QString::fromUtf8(
        u8"请重新思考这个历史局面，然后直接在棋盘上走出你认为最好的着法。"), panel);
    instruction->setWordWrap(true);
    panelLayout->addWidget(instruction);

    result_browser_ = new QTextBrowser(panel);
    result_browser_->setPlaceholderText(QString::fromUtf8(
        u8"完成落子后，这里会显示判定、历史走法和 Pikafish 推荐变化。"));
    panelLayout->addWidget(result_browser_, 1);

    auto *buttonRow = new QHBoxLayout;
    hint_button_ = new QPushButton(QString::fromUtf8(u8"提示"), panel);
    next_button_ = new QPushButton(QString::fromUtf8(u8"下一题"), panel);
    next_button_->setObjectName("trainingPrimary");
    auto *closeButton = new QPushButton(QString::fromUtf8(u8"结束训练"), panel);
    buttonRow->addWidget(hint_button_);
    buttonRow->addWidget(next_button_);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    panelLayout->addLayout(buttonRow);
    root->addWidget(panel, 2);

    setStyleSheet(QString::fromUtf8(R"(
        QDialog { background: #f4f1e9; color: #28241f; }
        QFrame#trainingCard {
            background: #fffdf8; border: 1px solid #ddd5c6; border-radius: 10px;
        }
        QLabel#trainingTitle { font-size: 20px; font-weight: 700; }
        QLabel#mutedText { color: #746d63; }
        QLabel#themeBadge {
            color: #8f382b; background: #f8e9e5; border: 1px solid #e7c3ba;
            border-radius: 6px; padding: 8px 10px; font-weight: 600;
        }
        QTextBrowser { border: 1px solid #ddd5c6; border-radius: 6px; background: white; padding: 8px; }
        QPushButton {
            min-height: 34px; padding: 0 14px; border-radius: 6px;
            border: 1px solid #cfc6b7; background: #fffdf8;
        }
        QPushButton:hover { background: #f0eadf; }
        QPushButton#trainingPrimary { color: white; background: #9b3f2f; border-color: #873426; }
        QPushButton:disabled { color: #999; background: #eeeae2; }
    )"));

    connect(board_, &XiangqiBoardWidget::trainingMoveMade,
            this, &TrainingDialog::handleMove);
    connect(hint_button_, &QPushButton::clicked,
            this, &TrainingDialog::showHint);
    connect(next_button_, &QPushButton::clicked,
            this, &TrainingDialog::nextPosition);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);

    loadSession();
}

void TrainingDialog::loadSession()
{
    QString error;
    const int generated = database_->generateTrainingPositions(user_id_, &error);
    if (generated < 0) {
        QMessageBox::warning(this, QString::fromUtf8(u8"生成训练题失败"), error);
    }
    positions_ = database_->dueTrainingPositions(user_id_, 10);
    if (positions_.isEmpty()) {
        progress_label_->setText(QString::fromUtf8(u8"今天没有到期的训练题"));
        theme_label_->setText(QString::fromUtf8(u8"继续完成对局，系统会从新的失误中生成训练。"));
        source_label_->clear();
        result_browser_->setHtml(QString::fromUtf8(
            u8"<h3>今日训练已完成</h3><p>已经掌握的题目会按照间隔复习计划再次出现。</p>"));
        board_->setEnabled(false);
        hint_button_->setEnabled(false);
        next_button_->setEnabled(false);
        return;
    }
    current_index_ = 0;
    loadCurrentPosition();
}

void TrainingDialog::loadCurrentPosition()
{
    if (current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_[current_index_];
    progress_label_->setText(QString::fromUtf8(u8"本次训练 %1 / %2 · 已掌握 %3 级")
                                 .arg(current_index_ + 1)
                                 .arg(positions_.size())
                                 .arg(position.mastery));
    theme_label_->setText(QString::fromUtf8(u8"训练主题：") + position.theme);
    source_label_->setText(QString::fromUtf8(
        u8"来自你的第 %1 盘、第 %2 步 · 当时局面损失 %3 · 已练习 %4 次")
        .arg(position.sourceGameId)
        .arg(position.sourcePly)
        .arg(position.scoreLoss)
        .arg(position.attempts));
    result_browser_->clear();
    hint_button_->setEnabled(true);
    next_button_->setEnabled(false);
    if (!board_->loadTrainingPosition(position.board.toStdString())) {
        result_browser_->setText(QString::fromUtf8(u8"无法加载这道训练题的局面。"));
        board_->setEnabled(false);
        next_button_->setEnabled(true);
        return;
    }
    board_->setEnabled(true);
    timer_.restart();
}

void TrainingDialog::handleMove(const QString &uciMove)
{
    if (current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_[current_index_];
    const bool correct = uciMove == position.bestMove;
    QString error;
    if (!database_->recordTrainingAttempt(position.id, uciMove, correct,
                                           timer_.elapsed(), &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"保存训练结果失败"), error);
    }

    const QString heading = correct
        ? QString::fromUtf8(u8"<h2 style='color:#26733c'>正确</h2>")
        : QString::fromUtf8(u8"<h2 style='color:#a43c30'>需要再练习</h2>");
    result_browser_->setHtml(
        heading +
        QString::fromUtf8(
            u8"<p>你的回答：<b>%1</b><br>本题目标着：<b>%2</b><br>"
            u8"你在实战中的走法：<b>%3</b></p>"
            u8"<p><b>Pikafish 推荐变化</b><br><code>%4</code></p>"
            u8"<p style='color:#746d63'>%5</p>")
            .arg(displayMove(uciMove).toHtmlEscaped(),
                 displayMove(position.bestMove).toHtmlEscaped(),
                 displayMove(position.actualMove).toHtmlEscaped(),
                 position.principalVariation.toHtmlEscaped(),
                 correct
                     ? QString::fromUtf8(u8"这道题将进入间隔复习计划。")
                     : QString::fromUtf8(u8"建议比较目标着与实战走法，检查自己遗漏了什么强制手段。")));
    hint_button_->setEnabled(false);
    next_button_->setEnabled(true);
}

void TrainingDialog::showHint()
{
    if (current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const QString bestMove = positions_[current_index_].bestMove;
    result_browser_->setHtml(QString::fromUtf8(
        u8"<p><b>提示：</b>请重点考虑从 <b>%1</b> 出发的棋子，先检查将军、吃子和直接威胁。</p>")
        .arg(bestMove.left(2).toHtmlEscaped()));
}

void TrainingDialog::nextPosition()
{
    ++current_index_;
    if (current_index_ >= positions_.size()) {
        const auto summary = database_->trainingSummary(user_id_);
        result_browser_->setHtml(QString::fromUtf8(
            u8"<h2>本次训练完成</h2><p>累计训练 %1 次，答对 %2 次；题库共有 %3 道个人错题。</p>")
            .arg(summary.attempts)
            .arg(summary.correctAttempts)
            .arg(summary.positions));
        progress_label_->setText(QString::fromUtf8(u8"训练完成"));
        theme_label_->setText(QString::fromUtf8(u8"系统会按照掌握度安排下次复习"));
        source_label_->clear();
        board_->setEnabled(false);
        hint_button_->setEnabled(false);
        next_button_->setEnabled(false);
        return;
    }
    loadCurrentPosition();
}

QString TrainingDialog::displayMove(const QString &uciMove)
{
    if (uciMove.size() < 4) {
        return uciMove;
    }
    return uciMove.left(2) + QString::fromUtf8(u8" → ") + uciMove.mid(2, 2);
}
