#include <QCoreApplication>
#include <QDebug>

#include "chess_score_importer.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    ChessScoreImporter::ParsedScore score;
    QString error;
    const QString text = QStringLiteral(
        "https://xiangqiai.com/#/rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/"
        "P1P1P1P1P/1C5C1/9/RNBAKABNR%20w%20moves%20c3c4b7c7h2e2");
    if (!ChessScoreImporter::parseText(text, QStringLiteral("sample.txt"), &score, &error)) {
        qCritical().noquote() << error;
        return 1;
    }
    if (score.sourceFormat != QStringLiteral("象棋AI链接（UCI）")
        || score.moves.size() != 3 || score.initialBoard.isEmpty()) {
        qCritical().noquote() << score.sourceFormat << score.moves << score.initialBoard;
        return 2;
    }
    ChessScoreImporter::ParsedScore positionOnly;
    if (!ChessScoreImporter::parseText(
            QStringLiteral("https://xiangqiai.com/#/5ab2/4acC2/3kb2N1/8p/p1pr5/1R1c5/P7P/8B/4A4/4KA3%20b"),
            QStringLiteral("position.txt"), &positionOnly, &error)
        || positionOnly.moves.size() != 0 || positionOnly.initialBoard.isEmpty()) {
        return 3;
    }

    // Optional real-file smoke checks used during development; the normal CTest
    // invocation remains self-contained and does not depend on user files.
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            ChessScoreImporter::ParsedScore realScore;
            if (!ChessScoreImporter::parseFile(QString::fromLocal8Bit(argv[i]), &realScore, &error)) {
                qCritical().noquote() << argv[i] << error;
                return 10 + i;
            }
            qInfo().noquote() << argv[i] << realScore.sourceFormat
                              << "moves=" << realScore.moves.size()
                              << "board=" << !realScore.initialBoard.isEmpty()
                              << "warning=" << realScore.warning;
        }
    }
    return 0;
}
