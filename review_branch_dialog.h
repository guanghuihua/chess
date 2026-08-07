#ifndef REVIEW_BRANCH_DIALOG_H
#define REVIEW_BRANCH_DIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class XiangqiBoardWidget;

class ReviewBranchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReviewBranchDialog(const QString &boardBefore, QWidget *parent = nullptr);

private:
    QString board_before_;
    XiangqiBoardWidget *board_ = nullptr;
    QLabel *status_label_ = nullptr;
};

#endif // REVIEW_BRANCH_DIALOG_H
