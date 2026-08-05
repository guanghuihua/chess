#include <QApplication>
#include <QStandardPaths>
#include <QTimer>

#include "game_database.h"
#include "training_dialog.h"
#include "xiangqi_game.h"

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("xiangqi-training-ui-smoke-test");
    QStandardPaths::setTestModeEnabled(true);

    GameDatabase database;
    QString error;
    if (!database.open(&error)) {
        return 1;
    }

    XiangqiGame game;
    if (!game.move(6, 0, 5, 0, 1800)) {
        return 2;
    }
    const auto &move = game.moveHistory().front();
    const qint64 gameId = database.startGame(1, &error);
    if (gameId < 0 || !database.recordMove(gameId, move, &error)) {
        return 3;
    }
    if (!database.recordAnalysis(gameId, 1, "a3a4", "b2b3",
                                 120, 0, 120, "mistake", "b2b3 b7b6", &error)) {
        return 4;
    }
    if (database.generateTrainingPositions(1, &error) < 0 ||
        database.dueTrainingPositions(1, 5).isEmpty()) {
        return 5;
    }

    TrainingDialog dialog(&database, 1);
    QTimer::singleShot(250, &dialog, &QDialog::accept);
    dialog.exec();
    return 0;
}
