from __future__ import annotations

import argparse
import math
import sys

try:
    from .board import Board, board_from_string, initial_board, opponent
    from .moves import Move, apply_move, generate_legal_moves
except ImportError:
    from board import Board, board_from_string, initial_board, opponent
    from moves import Move, apply_move, generate_legal_moves


PIECE_VALUES = {
    "K": 100_000,
    "R": 900,
    "C": 450,
    "H": 400,
    "E": 200,
    "A": 200,
    "S": 100,
}


def evaluate(board: Board, side: str) -> int:
    score = 0
    for row in board:
        for piece in row:
            if piece is None:
                continue
            value = PIECE_VALUES[piece.kind]
            score += value if piece.side == side else -value
    return score


def ordered_moves(board: Board, moves: list[Move]) -> list[Move]:
    return sorted(
        moves,
        key=lambda move: (
            PIECE_VALUES[board[move.to_row][move.to_col].kind]
            if board[move.to_row][move.to_col]
            else 0
        ),
        reverse=True,
    )


def negamax(
    board: Board, side: str, depth: int, alpha: int, beta: int
) -> tuple[int, Move | None]:
    if depth == 0:
        return evaluate(board, side), None

    moves = generate_legal_moves(board, side)
    if not moves:
        return -PIECE_VALUES["K"] - depth, None

    best_move: Move | None = None
    best_score = -math.inf
    for move in ordered_moves(board, moves):
        captured = apply_move(board, move)
        score, _ = negamax(board, opponent(side), depth - 1, -beta, -alpha)
        score = -score
        board[move.from_row][move.from_col] = board[move.to_row][move.to_col]
        board[move.to_row][move.to_col] = captured

        if score > best_score:
            best_score = score
            best_move = move
        alpha = max(alpha, score)
        if alpha >= beta:
            break
    return int(best_score), best_move


def choose_move(board: Board, side: str, depth: int) -> Move | None:
    _, move = negamax(board, side, max(1, depth), -math.inf, math.inf)
    return move


def parse_position(command: str) -> tuple[Board, str]:
    prefix = "position:"
    if not command.startswith(prefix):
        raise ValueError("expected 'position: <board> side:<red|black>'")

    payload = command[len(prefix) :].strip()
    marker = " side:"
    if marker not in payload:
        raise ValueError("position command is missing side")
    position, side = payload.rsplit(marker, 1)
    side = side.strip().lower()
    if side not in ("red", "black"):
        raise ValueError("side must be red or black")
    return board_from_string(position.strip()), side


def print_move(move: Move | None) -> None:
    if move is None:
        print("move: none", flush=True)
        return
    print(
        f"move: {move.from_row} {move.from_col} {move.to_row} {move.to_col}",
        flush=True,
    )


def run_protocol(depth: int) -> None:
    for raw_line in sys.stdin:
        command = raw_line.strip()
        if not command:
            continue
        if command == "quit":
            return
        if command == "ping":
            print("pong", flush=True)
            continue
        try:
            board, side = parse_position(command)
            print_move(choose_move(board, side, depth))
        except Exception as error:
            print(f"error: {error}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="A small Xiangqi engine")
    parser.add_argument("--protocol", action="store_true")
    parser.add_argument("--depth", type=int, default=2)
    arguments = parser.parse_args()

    if arguments.protocol:
        run_protocol(arguments.depth)
        return

    board = initial_board()
    print_move(choose_move(board, "black", arguments.depth))


if __name__ == "__main__":
    main()
