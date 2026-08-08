#include "chess_score_importer.h"

#include "xiangqi_game.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace {
QString expandFenBoard(const QString &encoded)
{
    QString expanded;
    for (const QChar ch : encoded) {
        if (ch.isDigit()) expanded += QString(ch.digitValue(), QLatin1Char('.'));
        else if (ch == QLatin1Char('N')) expanded += QLatin1Char('H');
        else if (ch == QLatin1Char('n')) expanded += QLatin1Char('h');
        else if (ch == QLatin1Char('B')) expanded += QLatin1Char('E');
        else if (ch == QLatin1Char('b')) expanded += QLatin1Char('e');
        else if (ch == QLatin1Char('P')) expanded += QLatin1Char('S');
        else if (ch == QLatin1Char('p')) expanded += QLatin1Char('s');
        else expanded += ch;
    }
    return expanded;
}

QString boardFromFragment(const QString &fragment, QString *side, QStringList *moves)
{
    QString value = fragment.trimmed();
    if (value.startsWith('/')) value.remove(0, 1);
    const QStringList parts = value.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.at(0).count('/') != 9) return {};
    if (side) *side = parts.value(1, QStringLiteral("w"));
    if (moves) {
        const int marker = parts.indexOf(QStringLiteral("moves"));
        for (int i = marker + 1; marker >= 0 && i < parts.size(); ++i) {
            const QString packed = parts.at(i);
            for (int offset = 0; offset + 4 <= packed.size(); offset += 4) {
                moves->append(packed.mid(offset, 4));
            }
        }
    }
    return expandFenBoard(parts.at(0));
}

QString boardFromText(const QString &text, QString *side)
{
    static const QRegularExpression boardPattern(
        QStringLiteral("([A-Za-z0-9]+(?:/[A-Za-z0-9]+){9})\\s+([wb])(?:\\s|$)"));
    const QRegularExpressionMatch match = boardPattern.match(text);
    if (!match.hasMatch()) return {};
    if (side) *side = match.captured(2);
    return expandFenBoard(match.captured(1));
}

bool validateMoves(const QString &board, const QString &sideText,
                   const QStringList &moves, QString *errorMessage)
{
    XiangqiGame game;
    const XiangqiGame::Side side = sideText.compare(QStringLiteral("b"), Qt::CaseInsensitive) == 0
        ? XiangqiGame::Side::Black : XiangqiGame::Side::Red;
    if (!game.loadPosition(board.toStdString(), side)) {
        if (errorMessage) *errorMessage = QString::fromUtf8(u8"初始局面不是合法的中国象棋局面。");
        return false;
    }
    for (const QString &move : moves) {
        if (move.size() != 4 || move.at(0) < 'a' || move.at(0) > 'i'
            || move.at(2) < 'a' || move.at(2) > 'i'
            || !move.at(1).isDigit() || !move.at(3).isDigit()) {
            if (errorMessage) *errorMessage = QString::fromUtf8(u8"棋谱中存在无法识别的坐标着法：") + move;
            return false;
        }
        const int fromRow = 9 - move.at(1).digitValue();
        const int toRow = 9 - move.at(3).digitValue();
        if (!game.move(fromRow, move.at(0).unicode() - 'a',
                       toRow, move.at(2).unicode() - 'a')) {
            if (errorMessage) *errorMessage = QString::fromUtf8(u8"第 ")
                + QString::number(game.moveHistory().size() + 1)
                + QString::fromUtf8(u8" 步不符合象棋规则：") + move;
            return false;
        }
    }
    return true;
}
}

bool ChessScoreImporter::parseFile(const QString &filePath, ParsedScore *score,
                                   QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.startsWith("GIF8")) {
        if (score) {
            score->sourceFile = QFileInfo(filePath).fileName();
            score->title = QFileInfo(filePath).completeBaseName();
            score->sourceFormat = QStringLiteral("GIF 动画");
            score->rawContent = QString::fromLatin1(bytes.toBase64());
            score->warning = QString::fromUtf8(u8"这是棋盘动画文件，已保存原文件，但暂不能从 GIF 自动恢复逐步棋谱。");
        }
        return true;
    }
    return parseText(QString::fromUtf8(bytes), QFileInfo(filePath).fileName(), score,
                     errorMessage);
}

bool ChessScoreImporter::parseText(const QString &text, const QString &sourceFile,
                                   ParsedScore *score, QString *errorMessage)
{
    if (!score) return false;
    *score = ParsedScore{};
    score->sourceFile = sourceFile;
    score->rawContent = text;
    score->title = QFileInfo(sourceFile).completeBaseName();
    QString plainSide;
    const QString plainBoard = boardFromText(text, &plainSide);
    if (!plainBoard.isEmpty() && validateMoves(plainBoard, plainSide, {}, errorMessage)) {
        score->sourceFormat = QStringLiteral("FEN 局面（原文着法）");
        score->initialBoard = plainBoard;
        score->sideToMove = plainSide;
        score->warning = QString::fromUtf8(u8"已识别初始局面；文件中的中文着法原文已保存，但暂未转换为可验证的坐标着法。");
    }
    auto urls = QRegularExpression(QStringLiteral("https?://[^\\s]+"))
        .globalMatch(text);
    while (urls.hasNext()) {
        const QString token = urls.next().captured(0);
        QString side;
        QStringList moves;
        const QString fragment = token.section(QLatin1Char('#'), 1);
        const QString decoded = QUrl::fromPercentEncoding(fragment.toUtf8());
        const QString board = boardFromFragment(decoded, &side, &moves);
        if (board.isEmpty()) continue;
        if (!moves.isEmpty()) {
            if (!validateMoves(board, side, moves, errorMessage)) return false;
            score->sourceFormat = QStringLiteral("象棋AI链接（UCI）");
            score->initialBoard = board;
            score->sideToMove = side;
            score->moves = moves;
            score->warning.clear();
            const QString beforeUrl = text.left(text.indexOf(token)).trimmed();
            if (!beforeUrl.isEmpty() && !beforeUrl.contains("http")) score->title = beforeUrl.split('\n').last().trimmed();
            return true;
        }
        if (score->initialBoard.isEmpty()) {
            if (!validateMoves(board, side, {}, errorMessage)) continue;
            score->sourceFormat = QStringLiteral("象棋AI链接（局面）");
            score->initialBoard = board;
            score->sideToMove = side;
        }
    }
    if (!score->initialBoard.isEmpty()) {
        if (score->warning.isEmpty()) {
            score->warning = QString::fromUtf8(u8"文件只包含初始局面，没有可复盘的着法。");
        }
        return true;
    }
    if (errorMessage) *errorMessage = QString::fromUtf8(u8"未识别出支持的象棋局面或棋谱链接。");
    return false;
}
