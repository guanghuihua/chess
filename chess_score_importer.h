#ifndef CHESS_SCORE_IMPORTER_H
#define CHESS_SCORE_IMPORTER_H

#include <QString>
#include <QVector>

class ChessScoreImporter
{
public:
    struct ParsedScore
    {
        QString title;
        QString sourceFormat;
        QString initialBoard;
        QString sideToMove;
        QStringList moves;
        QString rawContent;
        QString sourceFile;
        QString warning;
    };

    static bool parseFile(const QString &filePath, ParsedScore *score,
                          QString *errorMessage = nullptr);
    static bool parseText(const QString &text, const QString &sourceFile,
                          ParsedScore *score, QString *errorMessage = nullptr);
};

#endif
