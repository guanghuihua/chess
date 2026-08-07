#ifndef PIKAFISH_ANALYZER_H
#define PIKAFISH_ANALYZER_H

#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>

#include "xiangqi_game.h"

class PikafishAnalyzer : public QObject
{
    Q_OBJECT

public:
    struct AnalysisResult
    {
        qint64 gameId = -1;
        int ply = 0;
        QString actualMove;
        QString bestMove;
        QString actualNotation;
        QString bestNotation;
        int bestScore = 0;
        int actualScore = 0;
        int scoreLoss = 0;
        QString category;
        QString principalVariation;
        QString rawPrincipalVariation;
        QString actualPrincipalVariation;
        QString rawActualPrincipalVariation;
        XiangqiGame::Side sideToMove = XiangqiGame::Side::Red;
        QString explanation;
        QString boardBefore;
        qint64 thinkingTimeMs = 0;
    };

    struct PositionAnalysis
    {
        QString requestId;
        QString board;
        QString bestMove;
        QString rawPrincipalVariation;
        int score = 0;
        XiangqiGame::Side sideToMove = XiangqiGame::Side::Red;
    };

    explicit PikafishAnalyzer(QObject *parent = nullptr);
    ~PikafishAnalyzer() override;

    void analyzeMove(qint64 gameId, const XiangqiGame::MoveRecord &move);
    bool analyzeTrainingPosition(const QString &requestId, const QString &board,
                                 XiangqiGame::Side sideToMove,
                                 QString *errorMessage = nullptr);
    bool isAvailable() const;
    bool hasPendingAnalysis() const;
    QString enginePath() const;
    static QString toChineseNotation(const std::string &board,
                                     XiangqiGame::Side side,
                                     const QString &uciMove);
    static QString toChinesePrincipalVariation(const std::string &board,
                                               XiangqiGame::Side side,
                                               const QString &uciMoves);

signals:
    void analysisReady(const PikafishAnalyzer::AnalysisResult &result);
    void trainingPositionAnalyzed(const PikafishAnalyzer::PositionAnalysis &result,
                                  const QString &errorMessage);
    void analysisQueueDrained();
    void statusChanged(const QString &message, bool available);

private:
    enum class State
    {
        Stopped,
        WaitingForUci,
        WaitingForReady,
        Idle,
        AnalyzingBefore,
        AnalyzingAfter,
        AnalyzingPosition
    };

    struct Request
    {
        qint64 gameId = -1;
        XiangqiGame::MoveRecord move;
    };

    struct PositionRequest
    {
        QString requestId;
        QString board;
        XiangqiGame::Side sideToMove = XiangqiGame::Side::Red;
    };

    QProcess process_;
    QQueue<Request> requests_;
    QQueue<PositionRequest> position_requests_;
    QString output_buffer_;
    QString engine_path_;
    State state_ = State::Stopped;
    Request current_;
    PositionRequest current_position_;
    int latest_score_ = 0;
    QString latest_pv_;
    int best_score_ = 0;
    QString best_move_;
    QString best_pv_;

    void startEngine();
    void handleOutput();
    void handleLine(const QString &line);
    void processNextRequest();
    void beginPositionAnalysis();
    void finishPositionAnalysis(const QString &bestMove);
    void rejectPositionRequests(const QString &errorMessage);
    void beginBeforeAnalysis();
    void beginAfterAnalysis();
    void finishCurrentAnalysis();
    void sendCommand(const QString &command);

    static QString findEngineExecutable();
    static QString findProjectRoot();
    static QString toFen(const std::string &board, XiangqiGame::Side side, int ply);
    static QString toUciMove(const XiangqiGame::MoveRecord &move);
    static QString categoryForLoss(int loss);
    static QString explanationFor(const AnalysisResult &result);
    static int calculateScoreLoss(const AnalysisResult &result);
    static bool parseScoreAndPv(const QString &line, int *score, QString *pv);
};

Q_DECLARE_METATYPE(PikafishAnalyzer::AnalysisResult)
Q_DECLARE_METATYPE(PikafishAnalyzer::PositionAnalysis)

#endif // PIKAFISH_ANALYZER_H
