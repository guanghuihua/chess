#include "xiangqi_game.h"

#include <cassert>

int main()
{
    XiangqiGame game;
    const std::string initial = game.boardString();

    assert(game.move(6, 0, 5, 0));
    const std::string afterRed = game.boardString();
    assert(game.sideToMove() == XiangqiGame::Side::Black);

    assert(game.move(3, 0, 4, 0));
    assert(game.sideToMove() == XiangqiGame::Side::Red);
    assert(game.moveHistory().size() == 2);

    assert(game.undoLastMove());
    assert(game.boardString() == afterRed);
    assert(game.sideToMove() == XiangqiGame::Side::Black);
    assert(game.moveHistory().size() == 1);

    assert(game.undoLastMove());
    assert(game.boardString() == initial);
    assert(game.sideToMove() == XiangqiGame::Side::Red);
    assert(game.moveHistory().empty());
    assert(game.result() == XiangqiGame::GameResult::Ongoing);
    assert(!game.undoLastMove());

    assert(game.resign(XiangqiGame::Side::Red));
    assert(game.result() == XiangqiGame::GameResult::BlackWins);
    assert(!game.resign(XiangqiGame::Side::Red));

    game = XiangqiGame();
    assert(game.loadPosition(afterRed, XiangqiGame::Side::Black));
    assert(game.boardString() == afterRed);
    assert(game.sideToMove() == XiangqiGame::Side::Black);
    assert(game.moveHistory().empty());
    return 0;
}
