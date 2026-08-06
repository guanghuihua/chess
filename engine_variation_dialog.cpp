#include "engine_variation_dialog.h"

#include "pikafish_analyzer.h"
#include "xiangqi_board_widget.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

EngineVariationDialog::EngineVariationDialog(const QString &boardBefore,
                                             XiangqiGame::Side sideToMove,
                                             const QString &uciVariation,
                                             const QString &title,
                                             QWidget *parent)
    : QDialog(parent)
    , board_before_(boardBefore)
    , side_to_move_(sideToMove)
{
    setWindowTitle(title);
    resize(760, 870);
    setMinimumSize(620, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *heading = new QLabel(title, this);
    heading->setObjectName("variationTitle");
    layout->addWidget(heading);

    notice_label_ = new QLabel(this);
    notice_label_->setObjectName("variationNotice");
    notice_label_->setWordWrap(true);
    layout->addWidget(notice_label_);

    board_ = new XiangqiBoardWidget(this, false);
    board_->setMinimumSize(560, 620);
    layout->addWidget(board_, 1);

    progress_label_ = new QLabel(this);
    progress_label_->setObjectName("variationProgress");
    progress_label_->setAlignment(Qt::AlignCenter);
    layout->addWidget(progress_label_);

    variation_label_ = new QLabel(this);
    variation_label_->setObjectName("variationMoves");
    variation_label_->setWordWrap(true);
    variation_label_->setAlignment(Qt::AlignCenter);
    layout->addWidget(variation_label_);

    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(8);
    navigation->addStretch();
    const auto makeButton = [this, navigation](QStyle::StandardPixmap icon,
                                                const QString &tooltip,
                                                const char *objectName) {
        auto *button = new QToolButton(this);
        button->setObjectName(objectName);
        button->setIcon(style()->standardIcon(icon));
        button->setToolTip(tooltip);
        button->setAutoRaise(false);
        button->setFixedSize(42, 36);
        navigation->addWidget(button);
        return button;
    };
    first_button_ = makeButton(QStyle::SP_MediaSkipBackward,
                                QString::fromUtf8(u8"回到起始局面"),
                                "variationFirstButton");
    previous_button_ = makeButton(QStyle::SP_ArrowBack,
                                   QString::fromUtf8(u8"上一步"),
                                   "variationPreviousButton");
    next_button_ = makeButton(QStyle::SP_ArrowForward,
                               QString::fromUtf8(u8"下一步"),
                               "variationNextButton");
    last_button_ = makeButton(QStyle::SP_MediaSkipForward,
                               QString::fromUtf8(u8"跳到变化末端"),
                               "variationLastButton");
    navigation->addStretch();
    layout->addLayout(navigation);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);

    connect(first_button_, &QToolButton::clicked, this, &EngineVariationDialog::showFirst);
    connect(previous_button_, &QToolButton::clicked, this, &EngineVariationDialog::showPrevious);
    connect(next_button_, &QToolButton::clicked, this, &EngineVariationDialog::showNext);
    connect(last_button_, &QToolButton::clicked, this, &EngineVariationDialog::showLast);

    setStyleSheet(QString::fromUtf8(R"(
        QDialog { background:#f4f1e9; font-family:"Microsoft YaHei UI","Segoe UI"; }
        QLabel#variationTitle { font-size:18px; font-weight:700; color:#27363d; }
        QLabel#variationNotice { color:#5d625f; font-size:13px; }
        QLabel#variationProgress { color:#27363d; font-size:14px; font-weight:600; }
        QLabel#variationMoves { color:#56615c; font-size:13px; min-height:36px; }
        QToolButton { border:1px solid #c8c2b8; border-radius:5px; background:#fffdf8; }
        QToolButton:hover { background:#ece7db; border-color:#a59d90; }
        QToolButton:disabled { background:#efebe3; border-color:#ddd7cc; }
        QPushButton { min-height:32px; padding:0 18px; border:1px solid #c8c2b8;
                      border-radius:6px; background:#fffdf8; }
    )"));

    buildVariation(uciVariation);
    updatePosition();
}

int EngineVariationDialog::variationLength() const
{
    return variation_.size();
}

int EngineVariationDialog::currentPly() const
{
    return current_ply_;
}

void EngineVariationDialog::showFirst()
{
    current_ply_ = 0;
    updatePosition();
}

void EngineVariationDialog::showPrevious()
{
    if (current_ply_ > 0) {
        --current_ply_;
        updatePosition();
    }
}

void EngineVariationDialog::showNext()
{
    if (current_ply_ < variation_.size()) {
        ++current_ply_;
        updatePosition();
    }
}

void EngineVariationDialog::showLast()
{
    current_ply_ = variation_.size();
    updatePosition();
}

void EngineVariationDialog::buildVariation(const QString &uciVariation)
{
    XiangqiGame position;
    if (!position.loadPosition(board_before_.toStdString(), side_to_move_)) {
        notice_label_->setText(QString::fromUtf8(u8"无法载入引擎分析前的局面。"));
        return;
    }

    const QStringList moves = uciVariation.split(' ', Qt::SkipEmptyParts);
    for (const QString &uci : moves) {
        VariationMove step;
        step.uci = uci;
        if (!parseUciMove(uci, &step.fromRow, &step.fromCol, &step.toRow, &step.toCol)
            || !position.isLegalMove(step.fromRow, step.fromCol, step.toRow, step.toCol)) {
            notice_label_->setText(QString::fromUtf8(u8"引擎变化包含无法验证的着法，已在此前局面停止推演。"));
            break;
        }
        step.notation = PikafishAnalyzer::toChineseNotation(
            position.boardString(), position.sideToMove(), uci);
        if (!position.move(step.fromRow, step.fromCol, step.toRow, step.toCol)) {
            notice_label_->setText(QString::fromUtf8(u8"引擎变化未能通过规则校验，已停止推演。"));
            break;
        }
        variation_.append(step);
        if (position.result() != XiangqiGame::GameResult::Ongoing) {
            break;
        }
    }

    if (notice_label_->text().isEmpty()) {
        notice_label_->setText(variation_.isEmpty()
            ? QString::fromUtf8(u8"本次分析没有可推演的主变招。")
            : QString::fromUtf8(u8"按 Pikafish 主变招逐着查看，棋盘高亮当前一步的起点和终点。"));
    }
}

void EngineVariationDialog::updatePosition()
{
    XiangqiGame position;
    if (!position.loadPosition(board_before_.toStdString(), side_to_move_)) {
        return;
    }

    for (int index = 0; index < current_ply_; ++index) {
        const auto &step = variation_.at(index);
        if (!position.move(step.fromRow, step.fromCol, step.toRow, step.toCol)) {
            return;
        }
    }

    if (current_ply_ == 0) {
        board_->loadReviewPosition(position.boardString(), position.sideToMove());
    } else {
        const auto &step = variation_.at(current_ply_ - 1);
        board_->loadReviewPosition(position.boardString(), position.sideToMove(),
                                   step.fromRow, step.fromCol, step.toRow, step.toCol);
    }

    QStringList moves;
    for (int index = 0; index < variation_.size(); ++index) {
        const auto &step = variation_.at(index);
        const QString move = QString::fromUtf8(u8"%1. %2").arg(index + 1).arg(step.notation);
        moves.append(index + 1 == current_ply_ ? QStringLiteral("<b>%1</b>").arg(move)
                                               : move.toHtmlEscaped());
    }
    variation_label_->setText(moves.join(QString::fromUtf8(u8"　")));
    progress_label_->setText(current_ply_ == 0
        ? QString::fromUtf8(u8"分析前局面")
        : QString::fromUtf8(u8"主变招 %1 / %2").arg(current_ply_).arg(variation_.size()));
    updateControls();
}

void EngineVariationDialog::updateControls()
{
    first_button_->setEnabled(current_ply_ > 0);
    previous_button_->setEnabled(current_ply_ > 0);
    next_button_->setEnabled(current_ply_ < variation_.size());
    last_button_->setEnabled(current_ply_ < variation_.size());
}

bool EngineVariationDialog::parseUciMove(const QString &uci, int *fromRow, int *fromCol,
                                         int *toRow, int *toCol)
{
    if (uci.size() != 4 || uci[0] < QChar('a') || uci[0] > QChar('i')
        || uci[2] < QChar('a') || uci[2] > QChar('i')
        || uci[1] < QChar('0') || uci[1] > QChar('9')
        || uci[3] < QChar('0') || uci[3] > QChar('9')) {
        return false;
    }
    *fromCol = uci[0].unicode() - QChar('a').unicode();
    *fromRow = QChar('9').unicode() - uci[1].unicode();
    *toCol = uci[2].unicode() - QChar('a').unicode();
    *toRow = QChar('9').unicode() - uci[3].unicode();
    return XiangqiGame::inBounds(*fromRow, *fromCol)
        && XiangqiGame::inBounds(*toRow, *toCol);
}
