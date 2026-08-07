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
        u8"根据局面找出最好的着法，然后直接在棋盘上落子。"), panel);
    instruction->setWordWrap(true);
    panelLayout->addWidget(instruction);

    result_browser_ = new QTextBrowser(panel);
    result_browser_->setPlaceholderText(QString::fromUtf8(
        u8"完成落子后，这里会显示判定、历史走法和 Pikafish 推荐变化。"));
    panelLayout->addWidget(result_browser_, 1);

    auto *buttonRow = new QHBoxLayout;
    generate_button_ = new QPushButton(QString::fromUtf8(u8"AI 出新题"), panel);
    hint_button_ = new QPushButton(QString::fromUtf8(u8"提示"), panel);
    undo_button_ = new QPushButton(QString::fromUtf8(u8"悔棋"), panel);
    undo_button_->setObjectName("trainingUndo");
    next_button_ = new QPushButton(QString::fromUtf8(u8"下一题"), panel);
    ai_button_ = new QPushButton(QString::fromUtf8(u8"AI残局讲解"), panel);
    ai_button_->setObjectName("trainingAiButton");
    next_button_->setObjectName("trainingPrimary");
    auto *closeButton = new QPushButton(QString::fromUtf8(u8"结束训练"), panel);
    buttonRow->addWidget(hint_button_);
    buttonRow->addWidget(undo_button_);
    buttonRow->addWidget(generate_button_);
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
        QPushButton#trainingUndo { color: #8a4b22; border-color: #c9a98c; }
        QPushButton#trainingUndo:hover { background: #f6eadf; }
        QPushButton:disabled { color: #999; background: #eeeae2; }
    )"));

    connect(board_, &XiangqiBoardWidget::trainingMoveMade,
            this, &TrainingDialog::handleMove);
    connect(hint_button_, &QPushButton::clicked,
            this, &TrainingDialog::showHint);
    connect(undo_button_, &QPushButton::clicked,
            this, &TrainingDialog::undoCurrentMove);
    connect(generate_button_, &QPushButton::clicked,
            this, &TrainingDialog::requestGeneratedExercise);
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
        undo_button_->setEnabled(false);
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
    const bool isAiGenerated = position.sourcePly < 0;
    const bool isProfileVariation = position.theme.contains(QString::fromUtf8(u8"画像变式"));
    if (isAiGenerated) {
        sourceKind = QString::fromUtf8(u8"AI 原创题");
    } else if (isProfileVariation) {
        sourceKind = QString::fromUtf8(u8"画像原创变式");
    } else if (position.diagnosisTag == "undo_behavior") {
        sourceKind = QString::fromUtf8(u8"悔棋证据");
    } else if (position.diagnosisTag == "missed_mate"
               || position.theme.contains(QString::fromUtf8(u8"残局"))) {
        sourceKind = QString::fromUtf8(u8"残局/将杀专项");
    }
    theme_label_->setText(QString::fromUtf8(u8"训练主题：") + position.theme
                          + QString::fromUtf8(u8" · ") + sourceKind);
    const qint64 originPly = isProfileVariation ? position.sourcePly % 1000000
                                                 : position.sourcePly;
    if (isAiGenerated) {
        source_label_->setText(QString::fromUtf8(u8"这是一道根据你的错题模式与能力画像新生成的题目 · 已练习 %1 次\n出题依据：%2")
                                   .arg(position.attempts)
                                   .arg(position.recommendationReason));
    } else {
        source_label_->setText(QString::fromUtf8(
            u8"关联你的第 %1 盘、第 %2 步 · 当时局面损失 %3 · 已练习 %4 次")
            .arg(position.sourceGameId)
            .arg(originPly)
            .arg(position.scoreLoss)
            .arg(position.attempts)
            + QString::fromUtf8(u8"\n推荐原因：") + position.recommendationReason);
    }
    result_browser_->clear();
    pending_move_.clear();
    pending_move_thinking_time_ms_ = 0;
    hint_button_->setEnabled(true);
    undo_button_->setEnabled(false);
    ai_button_->setEnabled(position.theme.contains(QString::fromUtf8(u8"残局"))
                           || position.diagnosisTag == "missed_mate");
    next_button_->setEnabled(false);
    next_button_->setText(QString::fromUtf8(u8"下一题"));
    generate_button_->setEnabled(generation_request_id_.isEmpty());
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
    if (current_index_ < 0 || current_index_ >= positions_.size() || !pending_move_.isEmpty()) {
        return;
    }
    const auto &position = positions_[current_index_];
    pending_move_ = uciMove;
    pending_move_thinking_time_ms_ = timer_.elapsed();
    result_browser_->setHtml(QString::fromUtf8(
        u8"<h3>已落子，尚未提交</h3><p>你的尝试是 <b>%1</b>。确认作答后，系统才会判定并写入训练记录；"
        u8"若想重新计算，可点击“悔棋”。悔棋会撤回棋盘，但会按一次错误/不确定尝试计入个人训练统计。</p>")
                                 .arg(displayMove(position.board, uciMove).toHtmlEscaped()));
    hint_button_->setEnabled(false);
    ai_button_->setEnabled(false);
    undo_button_->setEnabled(true);
    next_button_->setText(QString::fromUtf8(u8"确认作答"));
    next_button_->setEnabled(true);
}

void TrainingDialog::confirmCurrentMove()
{
    if (pending_move_.isEmpty() || current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_[current_index_];
    const QString submittedMove = pending_move_;
    const bool correct = submittedMove == position.bestMove;
    QString error;
    if (!database_->recordTrainingAttempt(position.id, submittedMove, correct,
                                           pending_move_thinking_time_ms_, hint_count_, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"保存训练结果失败"), error);
        return;
    }
    pending_move_.clear();
    pending_move_thinking_time_ms_ = 0;

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
            .arg(displayMove(position.board, submittedMove).toHtmlEscaped(),
                 displayMove(position.board, position.bestMove).toHtmlEscaped(),
                 position.actualMove.isEmpty()
                     ? QString::fromUtf8(u8"本题为画像变式，没有历史实战着")
                     : displayMove(position.board, position.actualMove).toHtmlEscaped(),
                 displayVariation(position.board, position.principalVariation).toHtmlEscaped(),
                 correct
                     ? (hint_count_ == 0
                            ? QString::fromUtf8(u8"独立答对：掌握度上升，并进入间隔复习计划。")
                            : QString::fromUtf8(u8"借助 %1 级提示答对：系统已记录提示依赖，并会更早安排复测。")
                                  .arg(hint_count_))
                     : QString::fromUtf8(u8"建议比较目标着与实战走法，检查自己遗漏了什么强制手段。")));
    hint_button_->setEnabled(false);
    ai_button_->setEnabled(false);
    undo_button_->setEnabled(false);
    next_button_->setText(QString::fromUtf8(u8"下一题"));
    next_button_->setEnabled(true);
    requestAutomaticCoaching(submittedMove, correct);
}

void TrainingDialog::undoCurrentMove()
{
    if (pending_move_.isEmpty() || current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_[current_index_];
    QString error;
    if (!database_->recordTrainingAttempt(position.id, pending_move_, false,
                                           pending_move_thinking_time_ms_, hint_count_, &error)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"无法记录悔棋"), error);
        return;
    }
    if (!board_->undoTrainingMove()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"悔棋失败"),
                             QString::fromUtf8(u8"该尝试已记入训练记录，但棋盘未能恢复。请重新打开本题。"));
        return;
    }
    pending_move_.clear();
    pending_move_thinking_time_ms_ = 0;
    timer_.restart();
    hint_button_->setEnabled(hint_count_ < 3);
    ai_button_->setEnabled(position.theme.contains(QString::fromUtf8(u8"残局"))
                           || position.diagnosisTag == "missed_mate");
    undo_button_->setEnabled(false);
    next_button_->setText(QString::fromUtf8(u8"下一题"));
    next_button_->setEnabled(false);
    result_browser_->setHtml(QString::fromUtf8(
        u8"<h3>已悔棋</h3><p>这一步已撤回。按照训练规则，悔棋代表这次计算尚不确定，"
        u8"已按一次错误尝试写入个人训练统计；请重新计算后再确认作答。</p>"));
}

void TrainingDialog::requestAutomaticCoaching(const QString &uciMove, bool correct)
{
    if (!coach_request_id_.isEmpty() || current_index_ < 0 || current_index_ >= positions_.size()) {
        return;
    }
    const auto &position = positions_.at(current_index_);
    coach_request_id_ = QStringLiteral("training-feedback-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString evidence = QString::fromUtf8(
        u8"这是已经由 XiangqiGame 和 Pikafish 验证的专项训练。\n"
        u8"训练主题：%1\n用户画像标签：%2\n局面编码：%3\n"
        u8"用户作答：%4\n引擎最佳着：%5\n引擎主变：%6\n作答判定：%7")
        .arg(position.theme, position.diagnosisTag, position.board,
             displayMove(position.board, uciMove), displayMove(position.board, position.bestMove),
             displayVariation(position.board, position.principalVariation),
             correct ? QString::fromUtf8(u8"正确") : QString::fromUtf8(u8"错误"));
    result_browser_->append(QString::fromUtf8(
        u8"<p style='color:#315f54'><b>AI 教练正在根据你的落子解释原因……</b></p>"));
    emit coachQuestionAsked(coach_request_id_, evidence, QString(),
                            QString::fromUtf8(u8"请解释这次作答：错误时必须说明具体漏算、对手惩罚和正确着的作用；"
                                              u8"答对时说明关键判断为何成立，以及下次如何快速识别。"));
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
        const QString variation = displayVariation(position.board, position.principalVariation);
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
    if (!pending_move_.isEmpty()) {
        confirmCurrentMove();
        return;
    }
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
        undo_button_->setEnabled(false);
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
        .arg(position.theme, position.board,
             displayMove(position.board, position.bestMove),
             displayVariation(position.board, position.principalVariation),
             position.actualMove.isEmpty()
                 ? QString::fromUtf8(u8"无：这是画像变式题")
                 : displayMove(position.board, position.actualMove),
             position.diagnosisTag);
    result_browser_->setHtml(QString::fromUtf8(u8"<p>正在请求 GPT-5.6 Terra 生成残局杀法讲解……</p>"));
    ai_button_->setEnabled(false);
    emit coachQuestionAsked(
        coach_request_id_, evidence, QString(),
        QString::fromUtf8(u8"请把这个残局题讲成可执行的杀法训练：先说明双方将军、吃子和直接威胁，再给出我应计算的关键分支和成功标准。"));
}

void TrainingDialog::requestGeneratedExercise()
{
    if (!generation_request_id_.isEmpty()) return;
    generation_request_id_ = QStringLiteral("generated-training-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    generate_button_->setEnabled(false);
    result_browser_->setHtml(QString::fromUtf8(
        u8"<p>AI 正在根据你的错题模式和能力画像设计新题，随后会由 Pikafish 验题……</p>"));
    emit generatedExerciseRequested(generation_request_id_);
}

void TrainingDialog::generatedExerciseReady(const QString &requestId)
{
    if (requestId != generation_request_id_) return;
    generation_request_id_.clear();
    generate_button_->setEnabled(true);
    loadSession();
    result_browser_->setHtml(QString::fromUtf8(
        u8"<p style='color:#26733c'>AI 原创题已通过规则和 Pikafish 验证，已加入本次训练。</p>"));
}

void TrainingDialog::generatedExerciseFailed(const QString &requestId,
                                             const QString &errorMessage)
{
    if (requestId != generation_request_id_) return;
    generation_request_id_.clear();
    generate_button_->setEnabled(true);
    result_browser_->setHtml(QString::fromUtf8(
        u8"<p style='color:#9b342b'>本次 AI 候选题没有通过验证：%1</p>"
        u8"<p>没有将它保存到题库。可以再次生成另一题。</p>")
                                 .arg(errorMessage.toHtmlEscaped()));
}

void TrainingDialog::receiveCoachReply(const QString &requestId,
                                        const QString &answer,
                                        const QString &errorMessage)
{
    if (requestId != coach_request_id_) return;
    const bool isAutomaticFeedback = requestId.startsWith(QStringLiteral("training-feedback-"));
    coach_request_id_.clear();
    if (!errorMessage.isEmpty()) {
        result_browser_->append(QString::fromUtf8(
            u8"<p style='color:#9b342b'>AI 教练讲解失败：%1</p>")
                                     .arg(errorMessage.toHtmlEscaped()));
    } else {
        result_browser_->append(QString::fromUtf8(
            isAutomaticFeedback
                ? u8"<h3>AI 教练作答复盘</h3><p>%1</p>"
                : u8"<h3>GPT-5.6 Terra 残局杀法训练</h3><p>%1</p>")
                                     .arg(answer.toHtmlEscaped().replace("\n", "<br>")));
    }
    if (current_index_ >= 0 && current_index_ < positions_.size()) {
        const auto &position = positions_.at(current_index_);
        ai_button_->setEnabled(position.theme.contains(QString::fromUtf8(u8"残局"))
                               || position.diagnosisTag == "missed_mate");
    }
}

QString TrainingDialog::displayMove(const QString &board, const QString &uciMove)
{
    if (uciMove.size() != 4) {
        return uciMove;
    }
    return PikafishAnalyzer::toChineseNotation(
        board.toStdString(), XiangqiGame::Side::Red, uciMove);
}

QString TrainingDialog::displayVariation(const QString &board, const QString &variation)
{
    const QStringList moves = variation.split(' ', Qt::SkipEmptyParts);
    if (moves.isEmpty() || moves.front().size() != 4) return variation;
    return PikafishAnalyzer::toChinesePrincipalVariation(
        board.toStdString(), XiangqiGame::Side::Red, variation);
}
