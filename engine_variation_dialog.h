#ifndef ENGINE_VARIATION_DIALOG_H
#define ENGINE_VARIATION_DIALOG_H

#include <QDialog>
#include <QList>
#include <QStringList>

#include "xiangqi_game.h"

class QLabel;
class QToolButton;
class XiangqiBoardWidget;

class EngineVariationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EngineVariationDialog(const QString &boardBefore,
                                   XiangqiGame::Side sideToMove,
                                   const QString &uciVariation,
                                   const QString &title,
                                   QWidget *parent = nullptr);

    int variationLength() const;
    int currentPly() const;

private slots:
    void showFirst();
    void showPrevious();
    void showNext();
    void showLast();

private:
    struct VariationMove
    {
        QString uci;
        QString notation;
        int fromRow = -1;
        int fromCol = -1;
        int toRow = -1;
        int toCol = -1;
    };

    QString board_before_;
    XiangqiGame::Side side_to_move_;
    QList<VariationMove> variation_;
    int current_ply_ = 0;
    XiangqiBoardWidget *board_ = nullptr;
    QLabel *progress_label_ = nullptr;
    QLabel *variation_label_ = nullptr;
    QLabel *notice_label_ = nullptr;
    QToolButton *first_button_ = nullptr;
    QToolButton *previous_button_ = nullptr;
    QToolButton *next_button_ = nullptr;
    QToolButton *last_button_ = nullptr;

    void buildVariation(const QString &uciVariation);
    void updatePosition();
    void updateControls();
    static bool parseUciMove(const QString &uci, int *fromRow, int *fromCol,
                             int *toRow, int *toCol);
};

#endif // ENGINE_VARIATION_DIALOG_H
