#ifndef XIANGQI_GAME_H
#define XIANGQI_GAME_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class XiangqiGame
{
public:
    enum class Side { Red, Black };
    enum class PieceType { General, Advisor, Elephant, Horse, Rook, Cannon, Soldier };
    enum class GameResult { Ongoing, RedWins, BlackWins, Draw };

    struct Piece
    {
        PieceType type;
        Side side;
    };

    struct MoveRecord
    {
        int ply = 0;
        Side side = Side::Red;
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;
        Piece movedPiece{PieceType::Soldier, Side::Red};
        std::optional<Piece> capturedPiece;
        std::int64_t thinkingTimeMs = 0;
        std::string boardBefore;
        std::string boardAfter;
        GameResult resultAfter = GameResult::Ongoing;
    };

    XiangqiGame();

    const std::optional<Piece> &at(int row, int col) const;
    bool move(int fromRow, int fromCol, int toRow, int toCol,
              std::int64_t thinkingTimeMs = 0);
    bool resign(Side side);
    bool undoLastMove();
    bool loadPosition(const std::string &board, Side sideToMove);
    bool isLegalMove(int fromRow, int fromCol, int toRow, int toCol) const;
    Side sideToMove() const;
    GameResult result() const;
    std::string boardString() const;
    const std::vector<MoveRecord> &moveHistory() const;

    static bool inBounds(int row, int col);

private:
    using Board = std::array<std::array<std::optional<Piece>, 9>, 10>;

    Board board_{};
    Side side_to_move_ = Side::Red;
    GameResult result_ = GameResult::Ongoing;
    std::vector<MoveRecord> move_history_;

    bool isLegalMoveOnBoard(int fromRow, int fromCol, int toRow, int toCol,
                            Side side, const Board &board) const;
    bool isPseudoLegalMove(int fromRow, int fromCol, int toRow, int toCol,
                           const Board &board) const;
    bool isInCheck(Side side, const Board &board) const;
    bool attacksSquare(int fromRow, int fromCol, int toRow, int toCol,
                       const Board &board) const;
    bool hasAnyLegalMove(Side side) const;
    bool isGeneralPresent(Side side, const Board &board) const;

    static int countPiecesBetween(int fromRow, int fromCol, int toRow, int toCol,
                                  const Board &board);
    static bool isPathClear(int fromRow, int fromCol, int toRow, int toCol,
                            const Board &board);
    static bool isInPalace(Side side, int row, int col);
    static bool isOnOwnSide(Side side, int row);
    static bool hasCrossedRiver(Side side, int row);
};

#endif // XIANGQI_GAME_H
