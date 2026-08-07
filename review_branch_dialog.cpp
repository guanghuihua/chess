#include "review_branch_dialog.h"

#include "xiangqi_board_widget.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ReviewBranchDialog::ReviewBranchDialog(const QString &boardBefore, QWidget *parent)
    : QDialog(parent), board_before_(boardBefore)
{
    setWindowTitle(QString::fromUtf8(u8"复盘分支演练"));
    setMinimumSize(620, 720);
    resize(760, 850);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    auto *title = new QLabel(QString::fromUtf8(u8"尝试替代着法"), this);
    title->setStyleSheet("font-size:18px;font-weight:700;color:#27363d;");
    layout->addWidget(title);

    status_label_ = new QLabel(QString::fromUtf8(
        u8"这是临时分支，不会修改原对局。红方落子后，Pikafish 会像实战一样自动应对。"), this);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet("color:#5d625f;font-size:13px;");
    layout->addWidget(status_label_);

    board_ = new XiangqiBoardWidget(this, true);
    board_->setMinimumSize(560, 620);
    layout->addWidget(board_, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);

    setStyleSheet(QStringLiteral(
        "QDialog{background:#f4f1e9;font-family:\"Microsoft YaHei UI\",\"Segoe UI\";}"
        "QDialogButtonBox QPushButton{min-height:32px;padding:0 18px;border:1px solid #c8c2b8;"
        "border-radius:6px;background:#fffdf8;}"
        "QDialogButtonBox QPushButton:hover{background:#ece7db;}"));

    if (!board_->loadBranchPosition(board_before_.toStdString(), XiangqiGame::Side::Red)) {
        status_label_->setText(QString::fromUtf8(u8"当前局面无法通过规则校验，不能创建分支。"));
        board_->setEnabled(false);
    }
}
