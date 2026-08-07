#include "training_dialog.h"

#include "xiangqi_board_widget.h"
#include "pikafish_analyzer.h"

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUuid>

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

    auto *title = new QLabel(QString::fromUtf8(u8"个性化针对训练"), panel);
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
    ai_button_ = new QPushButton(QString::fromUtf8(u8"AI残局讲解"), panel);
    ai_button_->setObjectName("trainingAiButton");
    next_button_->setObjectName("trainingPrimary");
    auto *closeButton = new QPushButton(QString::fromUtf8(u8"结束训练"), panel);
    buttonRow->addWidget(hint_button_);
    buttonRow->addWidget(ai_button_);
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
        QPushButton#trainingAiButton { color: #315f54; border-color: #9bb8ac; }
        QPushButton#trainingAiButton:hover { background: #e5f0eb; }
        QPushButton:disabled { color: #999; background: #eeeae2; }
    )"));

    connect(board_, &XiangqiBoardWidget::trainingMoveMade,
            this, &TrainingDialog::handleMove);
    connect(hint_button_, &QPushButton::clicked,
            this, &TrainingDialog::showHint);
    connect(next_button_, &QPushButton::clicked,
            this, &TrainingDialog::nextPosition);
    connect(ai_button_, &QPushButton::clicked,
            this, &TrainingDialog::requestEndgameCoaching);
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
        ai_button_->setEnabled(false);
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
    QString sourceKind = QString::fromUtf8(u8"画像专项");
    if (position.diagnosisTag == "undo_behavior") {
        sourceKind = QString::fromUtf8(u8"悔棋证据");
    } else if (position.diagnosisTag == "missed_mate"
               || position.theme.contains(QString::fromUtf8(u8"残局"))) {
        sourceKind = QString::fromUtf8(u8"残局/将杀专项");
    }
    theme_label_->setText(QString::fromUtf8(u8"训练主题：") + position.theme
                          + QString::fromUtf8(u8" · ") + sourceKind);
    source_label_->setText(QString::fromUtf8(
        u8"来自你的第 %1 盘、第 %2 步 · 当时局面损失 %3 · 已练习 %4 次")
        .arg(position.sourceGameId)
        .arg(position.sourcePly)
        .arg(position.scoreLoss)
        .arg(position.attempts)
        + QString::fromUtf8(u8"\n推荐原因：") + position.recommendationReason);
    result_browser_->clear();
    hint_button_->setEnabled(true);
    ai_button_->setEnabled(position.theme.contains(QString::fromUtf8(u8"残局"))
                           || position.diagnosisTag == "missed_mate");
    next_button_->setEnabled(false);
    hint_count_ = 0;
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
                                           timer_.elapsed(), hint_count_, &error)) {
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
                     ? (hint_count_ == 0
                            ? QString::fromUtf8(u8"独立答对：掌握度上升，并进入间隔复习计划。")
                            : QString::fromUtf8(u8"借助 %1 级提示答对：系统已记录提示依赖，并会更早安排复测。")
                                  .arg(hint_count_))
                     : QString::fromUtf8(u8"建议比较目标着与实战走法，检查自己遗漏了什么强制手段。")));
    hint_button_->setEnabled(false);
    ai_button_->setEnabled(false);
    next_button_->setEnabled(true);
}

void TrainingDialog::showHint()
{
    if (current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_[current_index_];
    hint_count_ = std::min(3, hint_count_ + 1);
    QString hint;
    if (hint_count_ == 1) {
        if (position.diagnosisTag == "missed_mate") {
            hint = QString::fromUtf8(u8"方向提示：先列出所有将军手段，寻找对方无法化解的连续变化。");
        } else if (position.diagnosisTag == "missed_threat") {
            hint = QString::fromUtf8(u8"方向提示：先不要考虑自己的计划，检查对方下一步的将军、吃子和直接威胁。");
        } else {
            hint = QString::fromUtf8(u8"方向提示：写出至少两个候选着，并分别寻找对方最强回应。");
        }
    } else if (hint_count_ == 2) {
        hint = QString::fromUtf8(u8"局部提示：重点考虑从 <b>%1</b> 出发的棋子。").arg(
            position.bestMove.left(2).toHtmlEscaped());
    } else {
        const QString notation = PikafishAnalyzer::toChineseNotation(
            position.board.toStdString(), XiangqiGame::Side::Red, position.bestMove);
        const QString variation = PikafishAnalyzer::toChinesePrincipalVariation(
            position.board.toStdString(), XiangqiGame::Side::Red,
            position.principalVariation);
        hint = QString::fromUtf8(
            u8"计算提示：推荐着是 <b>%1</b>；关键变化为：%2。请先解释为什么，再落子。").arg(
            notation.toHtmlEscaped(), variation.toHtmlEscaped());
    }
    result_browser_->setHtml(QString::fromUtf8(u8"<h3>第 %1 级提示</h3><p>%2</p>")
                                 .arg(hint_count_).arg(hint));
    if (hint_count_ >= 3) hint_button_->setEnabled(false);
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
        ai_button_->setEnabled(false);
        next_button_->setEnabled(false);
        return;
    }
    loadCurrentPosition();
}

void TrainingDialog::requestEndgameCoaching()
{
    if (!coach_request_id_.isEmpty() || current_index_ < 0
        || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_.at(current_index_);
    coach_request_id_ = QStringLiteral("training-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString evidence = QString::fromUtf8(
        u8"这是程序已经通过 XiangqiGame 合法性检查、并由 Pikafish 计算的残局/将杀训练局面。"
        u8"请只根据给定证据讲解，不要编造未提供的着法。\n"
        u8"训练主题：%1\n局面编码：%2\nPikafish 最佳着：%3\n主变：%4\n"
        u8"用户实战着：%5\n画像标签：%6")
        .arg(position.theme, position.board, position.bestMove,
             position.principalVariation, position.actualMove,
             position.diagnosisTag);
    result_browser_->setHtml(QString::fromUtf8(u8"<p>正在请求 GPT-5.6 Sol 生成残局杀法讲解……</p>"));
    ai_button_->setEnabled(false);
    emit coachQuestionAsked(
        coach_request_id_, evidence, QString(),
        QString::fromUtf8(u8"请把这个残局题讲成可执行的杀法训练：先说明双方将军、吃子和直接威胁，再给出我应计算的关键分支和成功标准。"));
}

void TrainingDialog::receiveCoachReply(const QString &requestId,
                                        const QString &answer,
                                        const QString &errorMessage)
{
    if (requestId != coach_request_id_) return;
    coach_request_id_.clear();
    if (!errorMessage.isEmpty()) {
        result_browser_->setHtml(QString::fromUtf8(
            u8"<p style='color:#9b342b'>AI残局讲解失败：%1</p>")
                                     .arg(errorMessage.toHtmlEscaped()));
    } else {
        result_browser_->setHtml(QString::fromUtf8(
            u8"<h3>GPT-5.6 Sol 残局杀法训练</h3><p>%1</p>")
                                     .arg(answer.toHtmlEscaped().replace("\n", "<br>")));
    }
    if (current_index_ >= 0 && current_index_ < positions_.size()) {
        const auto &position = positions_.at(current_index_);
        ai_button_->setEnabled(position.theme.contains(QString::fromUtf8(u8"残局"))
                               || position.diagnosisTag == "missed_mate");
    }
}

QString TrainingDialog::displayMove(const QString &uciMove)
{
    if (uciMove.size() < 4) {
        return uciMove;
    }
    return uciMove.left(2) + QString::fromUtf8(u8" → ") + uciMove.mid(2, 2);
}
