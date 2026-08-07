#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#define private public
#include "deepseek_coach.h"
#undef private

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    PikafishAnalyzer::AnalysisResult analysis;
    analysis.ply = 12;
    analysis.actualMove = QStringLiteral("a3a4");
    analysis.bestMove = QStringLiteral("b2b3");
    analysis.actualNotation = QString::fromUtf8(u8"炮二平五");
    analysis.bestNotation = QString::fromUtf8(u8"车九进一");
    analysis.boardBefore = QStringLiteral("RAW_BOARD_MUST_NOT_LEAK");
    analysis.bestScore = 35;
    analysis.actualScore = -80;
    analysis.scoreLoss = 115;
    analysis.category = QStringLiteral("blunder");
    analysis.thinkingTimeMs = 1500;
    analysis.actualPrincipalVariation = QString::fromUtf8(u8"炮二平五 马８进７");
    analysis.principalVariation = QString::fromUtf8(u8"车九进一 车１平２");

    GameDatabase::TrainingStats stats;
    DeepSeekCoach::Request request{analysis, stats, QStringLiteral("我只看到一步将军")};
    const QJsonArray messages = QJsonDocument::fromJson(
        DeepSeekCoach::makeRequestBody(request)).object().value("messages").toArray();
    if (messages.size() != 2) return 1;

    const QString system = messages.at(0).toObject().value("content").toString();
    const QString user = messages.at(1).toObject().value("content").toString();
    if (user.contains(analysis.actualMove) || user.contains(analysis.bestMove)
        || user.contains(analysis.boardBefore)) {
        return 2;
    }
    if (!user.contains(analysis.actualNotation) || !user.contains(analysis.bestNotation)
        || !user.contains(analysis.actualPrincipalVariation)
        || !user.contains(analysis.principalVariation)) {
        return 3;
    }
    if (!system.contains(QString::fromUtf8(u8"不得根据记忆、坐标棋谱或棋盘编码推测棋子位置"))) {
        return 4;
    }
    return 0;
}
