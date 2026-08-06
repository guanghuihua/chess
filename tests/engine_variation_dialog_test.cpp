#include <QApplication>
#include <QToolButton>

#include "engine_variation_dialog.h"

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    XiangqiGame initial;
    EngineVariationDialog dialog(
        QString::fromStdString(initial.boardString()), XiangqiGame::Side::Red,
        "a3a4 a6a5", QStringLiteral("test"));
    if (dialog.variationLength() != 2 || dialog.currentPly() != 0) {
        return 1;
    }

    auto *next = dialog.findChild<QToolButton *>("variationNextButton");
    auto *last = dialog.findChild<QToolButton *>("variationLastButton");
    auto *previous = dialog.findChild<QToolButton *>("variationPreviousButton");
    if (!next || !last || !previous) {
        return 2;
    }
    next->click();
    if (dialog.currentPly() != 1) {
        return 3;
    }
    last->click();
    if (dialog.currentPly() != 2) {
        return 4;
    }
    previous->click();
    if (dialog.currentPly() != 1) {
        return 5;
    }

    const QString capturePath = qEnvironmentVariable("MINDDUET_VARIATION_CAPTURE");
    if (!capturePath.isEmpty()) {
        dialog.resize(760, 870);
        dialog.show();
        application.processEvents();
        if (!dialog.grab().save(capturePath)) {
            return 6;
        }
    }

    EngineVariationDialog invalidDialog(
        QString::fromStdString(initial.boardString()), XiangqiGame::Side::Red,
        "a3a5", QStringLiteral("test"));
    return invalidDialog.variationLength() == 0 ? 0 : 7;
}
