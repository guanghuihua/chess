#include "xiangqi_game.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace {
char pieceTypeCode(XiangqiGame::PieceType type)
{
    switch (type) {
    case XiangqiGame::PieceType::General: return 'K';
    case XiangqiGame::PieceType::Advisor: return 'A';
    case XiangqiGame::PieceType::Elephant: return 'E';
    case XiangqiGame::PieceType::Horse: return 'H';
    case XiangqiGame::PieceType::Rook: return 'R';
    case XiangqiGame::PieceType::Cannon: return 'C';
    case XiangqiGame::PieceType::Soldier: return 'S';
    }
    return '?';
}
}

XiangqiGame::XiangqiGame()
{
    auto place = [this](int row, int col, PieceType type, Side side) {
        board_[row][col] = Piece{type, side};
    };

    const std::array<PieceType, 9> backRank = {
        PieceType::Rook, PieceType::Horse, PieceType::Elephant,
        PieceType::Advisor, PieceType::General, PieceType::Advisor,
        PieceType::Elephant, PieceType::Horse, PieceType::Rook
    };

    for (int col = 0; col < 9; ++col) {
        place(0, col, backRank[col], Side::Black);
        place(9, col, backRank[col], Side::Red);
    }
    place(2, 1, PieceType::Cannon, Side::Black);
    place(2, 7, PieceType::Cannon, Side::Black);
    place(7, 1, PieceType::Cannon, Side::Red);
    place(7, 7, PieceType::Cannon, Side::Red);
    for (int col = 0; col < 9; col += 2) {
        place(3, col, PieceType::Soldier, Side::Black);
        place(6, col, PieceType::Soldier, Side::Red);
    }
}

const std::optional<XiangqiGame::Piece> &XiangqiGame::at(int row, int col) const
{
    return board_[row][col];
}

bool XiangqiGame::move(int fromRow, int fromCol, int toRow, int toCol,
                       std::int64_t thinkingTimeMs)
{
    if (result_ != GameResult::Ongoing ||
        !isLegalMove(fromRow, fromCol, toRow, toCol)) {
        return false;
    }

    const Side movingSide = side_to_move_;
    MoveRecord record;
    record.ply = static_cast<int>(move_history_.size()) + 1;
    record.side = movingSide;
    record.fromRow = fromRow;
    record.fromCol = fromCol;
    record.toRow = toRow;
    record.toCol = toCol;
    record.movedPiece = *board_[fromRow][fromCol];
    record.capturedPiece = board_[toRow][toCol];
    record.thinkingTimeMs = std::max<std::int64_t>(0, thinkingTimeMs);
    record.boardBefore = boardString();

    board_[toRow][toCol] = board_[fromRow][fromCol];
    board_[fromRow][fromCol].reset();
    side_to_move_ = movingSide == Side::Red ? Side::Black : Side::Red;

    if (!isGeneralPresent(side_to_move_, board_) || !hasAnyLegalMove(side_to_move_)) {
        result_ = movingSide == Side::Red ? GameResult::RedWins : GameResult::BlackWins;
    }

    record.boardAfter = boardString();
    record.resultAfter = result_;
    move_history_.push_back(std::move(record));

    return true;
}

bool XiangqiGame::resign(Side side)
{
    if (result_ != GameResult::Ongoing) {
        return false;
    }
    result_ = side == Side::Red ? GameResult::BlackWins : GameResult::RedWins;
    return true;
}

bool XiangqiGame::undoLastMove()
{
    if (move_history_.empty()) {
        return false;
    }

    const MoveRecord &record = move_history_.back();
    board_[record.fromRow][record.fromCol] = record.movedPiece;
    board_[record.toRow][record.toCol] = record.capturedPiece;
    side_to_move_ = record.side;
    result_ = GameResult::Ongoing;
    move_history_.pop_back();
    return true;
}

bool XiangqiGame::loadPosition(const std::string &board, Side sideToMove)
{
    Board loaded{};
    int row = 0;
    int col = 0;
    for (char raw : board) {
        if (raw == '/') {
            if (col != 9 || row >= 9) {
                return false;
            }
            ++row;
            col = 0;
            continue;
        }
        if (row >= 10 || col >= 9) {
            return false;
        }
        if (raw != '.') {
            const bool red = std::isupper(static_cast<unsigned char>(raw)) != 0;
            PieceType type;
            switch (static_cast<char>(std::toupper(static_cast<unsigned char>(raw)))) {
            case 'K': type = PieceType::General; break;
            case 'A': type = PieceType::Advisor; break;
            case 'E': type = PieceType::Elephant; break;
            case 'H': type = PieceType::Horse; break;
            case 'R': type = PieceType::Rook; break;
            case 'C': type = PieceType::Cannon; break;
            case 'S': type = PieceType::Soldier; break;
            default: return false;
            }
            loaded[row][col] = Piece{type, red ? Side::Red : Side::Black};
        }
        ++col;
    }
    if (row != 9 || col != 9) {
        return false;
    }

    board_ = loaded;
    side_to_move_ = sideToMove;
    result_ = GameResult::Ongoing;
    move_history_.clear();
    return isGeneralPresent(Side::Red, board_) &&
           isGeneralPresent(Side::Black, board_);
}

bool XiangqiGame::isLegalMove(int fromRow, int fromCol, int toRow, int toCol) const
{
    return isLegalMoveOnBoard(fromRow, fromCol, toRow, toCol, side_to_move_, board_);
}

XiangqiGame::Side XiangqiGame::sideToMove() const
{
    return side_to_move_;
}

XiangqiGame::GameResult XiangqiGame::result() const
{
    return result_;
}

std::string XiangqiGame::boardString() const
{
    std::string result;
    result.reserve(99);
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 9; ++col) {
            char code = '.';
            if (board_[row][col].has_value()) {
                code = pieceTypeCode(board_[row][col]->type);
                if (board_[row][col]->side == Side::Black) {
                    code = static_cast<char>(std::tolower(static_cast<unsigned char>(code)));
                }
            }
            result.push_back(code);
        }
        if (row != 9) {
            result.push_back('/');
        }
    }
    return result;
}

const std::vector<XiangqiGame::MoveRecord> &XiangqiGame::moveHistory() const
{
    return move_history_;
}

bool XiangqiGame::inBounds(int row, int col)
{
    return row >= 0 && row < 10 && col >= 0 && col < 9;
}

bool XiangqiGame::isLegalMoveOnBoard(int fromRow, int fromCol, int toRow, int toCol,
                                     Side side, const Board &board) const
{
    if (!inBounds(fromRow, fromCol) || !inBounds(toRow, toCol) ||
        (fromRow == toRow && fromCol == toCol)) {
        return false;
    }

    const auto &piece = board[fromRow][fromCol];
    const auto &target = board[toRow][toCol];
    if (!piece.has_value() || piece->side != side ||
        (target.has_value() && target->side == side) ||
        !isPseudoLegalMove(fromRow, fromCol, toRow, toCol, board)) {
        return false;
    }

    Board copy = board;
    copy[toRow][toCol] = copy[fromRow][fromCol];
    copy[fromRow][fromCol].reset();
    return !isInCheck(side, copy);
}

bool XiangqiGame::isPseudoLegalMove(int fromRow, int fromCol, int toRow, int toCol,
                                    const Board &board) const
{
    const Piece piece = *board[fromRow][fromCol];
    const auto &target = board[toRow][toCol];
    const int deltaRow = toRow - fromRow;
    const int deltaCol = toCol - fromCol;
    const int rowDistance = std::abs(deltaRow);
    const int colDistance = std::abs(deltaCol);

    switch (piece.type) {
    case PieceType::Rook:
        return (deltaRow == 0 || deltaCol == 0) &&
               isPathClear(fromRow, fromCol, toRow, toCol, board);
    case PieceType::Cannon: {
        if (deltaRow != 0 && deltaCol != 0) {
            return false;
        }
        const int screens = countPiecesBetween(fromRow, fromCol, toRow, toCol, board);
        return target.has_value() ? screens == 1 : screens == 0;
    }
    case PieceType::Horse: {
        if (!((rowDistance == 2 && colDistance == 1) ||
              (rowDistance == 1 && colDistance == 2))) {
            return false;
        }
        const int legRow = rowDistance == 2 ? fromRow + deltaRow / 2 : fromRow;
        const int legCol = colDistance == 2 ? fromCol + deltaCol / 2 : fromCol;
        return !board[legRow][legCol].has_value();
    }
    case PieceType::Elephant: {
        if (rowDistance != 2 || colDistance != 2 || !isOnOwnSide(piece.side, toRow)) {
            return false;
        }
        return !board[fromRow + deltaRow / 2][fromCol + deltaCol / 2].has_value();
    }
    case PieceType::Advisor:
        return rowDistance == 1 && colDistance == 1 &&
               isInPalace(piece.side, toRow, toCol);
    case PieceType::General:
        if (rowDistance + colDistance == 1) {
            return isInPalace(piece.side, toRow, toCol);
        }
        return deltaCol == 0 && target.has_value() &&
               target->type == PieceType::General &&
               isPathClear(fromRow, fromCol, toRow, toCol, board);
    case PieceType::Soldier:
        if (rowDistance + colDistance != 1) {
            return false;
        }
        if (piece.side == Side::Red && deltaRow > 0) {
            return false;
        }
        if (piece.side == Side::Black && deltaRow < 0) {
            return false;
        }
        return deltaCol == 0 || hasCrossedRiver(piece.side, fromRow);
    }
    return false;
}

bool XiangqiGame::isInCheck(Side side, const Board &board) const
{
    int generalRow = -1;
    int generalCol = -1;
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 9; ++col) {
            const auto &piece = board[row][col];
            if (piece.has_value() && piece->side == side &&
                piece->type == PieceType::General) {
                generalRow = row;
                generalCol = col;
            }
        }
    }
    if (generalRow < 0) {
        return true;
    }

    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 9; ++col) {
            const auto &piece = board[row][col];
            if (piece.has_value() && piece->side != side &&
                attacksSquare(row, col, generalRow, generalCol, board)) {
                return true;
            }
        }
    }
    return false;
}

bool XiangqiGame::attacksSquare(int fromRow, int fromCol, int toRow, int toCol,
                                const Board &board) const
{
    const Piece piece = *board[fromRow][fromCol];
    const int deltaRow = toRow - fromRow;
    const int deltaCol = toCol - fromCol;
    const int rowDistance = std::abs(deltaRow);
    const int colDistance = std::abs(deltaCol);

    switch (piece.type) {
    case PieceType::Rook:
        return (deltaRow == 0 || deltaCol == 0) &&
               isPathClear(fromRow, fromCol, toRow, toCol, board);
    case PieceType::Cannon:
        return (deltaRow == 0 || deltaCol == 0) &&
               countPiecesBetween(fromRow, fromCol, toRow, toCol, board) == 1;
    case PieceType::Horse: {
        if (!((rowDistance == 2 && colDistance == 1) ||
              (rowDistance == 1 && colDistance == 2))) {
            return false;
        }
        const int legRow = rowDistance == 2 ? fromRow + deltaRow / 2 : fromRow;
        const int legCol = colDistance == 2 ? fromCol + deltaCol / 2 : fromCol;
        return !board[legRow][legCol].has_value();
    }
    case PieceType::Elephant:
        return rowDistance == 2 && colDistance == 2 &&
               isOnOwnSide(piece.side, toRow) &&
               !board[fromRow + deltaRow / 2][fromCol + deltaCol / 2].has_value();
    case PieceType::Advisor:
        return rowDistance == 1 && colDistance == 1 &&
               isInPalace(piece.side, toRow, toCol);
    case PieceType::General:
        if (rowDistance + colDistance == 1) {
            return isInPalace(piece.side, toRow, toCol);
        }
        return deltaCol == 0 && isPathClear(fromRow, fromCol, toRow, toCol, board);
    case PieceType::Soldier:
        if (rowDistance + colDistance != 1) {
            return false;
        }
        if ((piece.side == Side::Red && deltaRow > 0) ||
            (piece.side == Side::Black && deltaRow < 0)) {
            return false;
        }
        return deltaCol == 0 || hasCrossedRiver(piece.side, fromRow);
    }
    return false;
}

bool XiangqiGame::hasAnyLegalMove(Side side) const
{
    for (int fromRow = 0; fromRow < 10; ++fromRow) {
        for (int fromCol = 0; fromCol < 9; ++fromCol) {
            if (!board_[fromRow][fromCol].has_value() ||
                board_[fromRow][fromCol]->side != side) {
                continue;
            }
            for (int toRow = 0; toRow < 10; ++toRow) {
                for (int toCol = 0; toCol < 9; ++toCol) {
                    if (isLegalMoveOnBoard(fromRow, fromCol, toRow, toCol, side, board_)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool XiangqiGame::isGeneralPresent(Side side, const Board &board) const
{
    for (const auto &row : board) {
        for (const auto &piece : row) {
            if (piece.has_value() && piece->side == side &&
                piece->type == PieceType::General) {
                return true;
            }
        }
    }
    return false;
}

int XiangqiGame::countPiecesBetween(int fromRow, int fromCol, int toRow, int toCol,
                                    const Board &board)
{
    int count = 0;
    if (fromRow == toRow) {
        const int step = toCol > fromCol ? 1 : -1;
        for (int col = fromCol + step; col != toCol; col += step) {
            count += board[fromRow][col].has_value() ? 1 : 0;
        }
    } else if (fromCol == toCol) {
        const int step = toRow > fromRow ? 1 : -1;
        for (int row = fromRow + step; row != toRow; row += step) {
            count += board[row][fromCol].has_value() ? 1 : 0;
        }
    }
    return count;
}

bool XiangqiGame::isPathClear(int fromRow, int fromCol, int toRow, int toCol,
                              const Board &board)
{
    return countPiecesBetween(fromRow, fromCol, toRow, toCol, board) == 0;
}

bool XiangqiGame::isInPalace(Side side, int row, int col)
{
    if (col < 3 || col > 5) {
        return false;
    }
    return side == Side::Red ? row >= 7 && row <= 9 : row >= 0 && row <= 2;
}

bool XiangqiGame::isOnOwnSide(Side side, int row)
{
    return side == Side::Red ? row >= 5 : row <= 4;
}

bool XiangqiGame::hasCrossedRiver(Side side, int row)
{
    return !isOnOwnSide(side, row);
}
