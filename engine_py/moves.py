from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator

try:
    from .board import Board, Piece, clone_board, find_general, in_bounds, opponent
except ImportError:
    from board import Board, Piece, clone_board, find_general, in_bounds, opponent


@dataclass(frozen=True)
class Move:
    from_row: int
    from_col: int
    to_row: int
    to_col: int


ORTHOGONAL_DIRECTIONS = ((-1, 0), (1, 0), (0, -1), (0, 1))
HORSE_STEPS = (
    (-2, -1, -1, 0),
    (-2, 1, -1, 0),
    (2, -1, 1, 0),
    (2, 1, 1, 0),
    (-1, -2, 0, -1),
    (1, -2, 0, -1),
    (-1, 2, 0, 1),
    (1, 2, 0, 1),
)


def is_in_palace(side: str, row: int, col: int) -> bool:
    if not 3 <= col <= 5:
        return False
    return 7 <= row <= 9 if side == "red" else 0 <= row <= 2


def has_crossed_river(side: str, row: int) -> bool:
    return row <= 4 if side == "red" else row >= 5


def is_on_own_side(side: str, row: int) -> bool:
    return row >= 5 if side == "red" else row <= 4


def can_land(board: Board, side: str, row: int, col: int) -> bool:
    if not in_bounds(row, col):
        return False
    target = board[row][col]
    return target is None or target.side != side


def ray_moves(
    board: Board, row: int, col: int, side: str, cannon: bool
) -> Iterator[Move]:
    for row_step, col_step in ORTHOGONAL_DIRECTIONS:
        target_row = row + row_step
        target_col = col + col_step
        screen_found = False
        while in_bounds(target_row, target_col):
            target = board[target_row][target_col]
            if not cannon:
                if target is None:
                    yield Move(row, col, target_row, target_col)
                else:
                    if target.side != side:
                        yield Move(row, col, target_row, target_col)
                    break
            elif not screen_found:
                if target is None:
                    yield Move(row, col, target_row, target_col)
                else:
                    screen_found = True
            elif target is not None:
                if target.side != side:
                    yield Move(row, col, target_row, target_col)
                break
            target_row += row_step
            target_col += col_step


def candidate_moves(board: Board, row: int, col: int) -> Iterator[Move]:
    piece = board[row][col]
    if piece is None:
        return

    side = piece.side
    kind = piece.kind
    if kind in ("R", "C"):
        yield from ray_moves(board, row, col, side, kind == "C")
        return

    if kind == "H":
        for row_delta, col_delta, leg_row, leg_col in HORSE_STEPS:
            target_row = row + row_delta
            target_col = col + col_delta
            if (
                can_land(board, side, target_row, target_col)
                and board[row + leg_row][col + leg_col] is None
            ):
                yield Move(row, col, target_row, target_col)
        return

    if kind == "E":
        for row_delta, col_delta in ((-2, -2), (-2, 2), (2, -2), (2, 2)):
            target_row = row + row_delta
            target_col = col + col_delta
            eye_row = row + row_delta // 2
            eye_col = col + col_delta // 2
            if (
                can_land(board, side, target_row, target_col)
                and is_on_own_side(side, target_row)
                and board[eye_row][eye_col] is None
            ):
                yield Move(row, col, target_row, target_col)
        return

    if kind == "A":
        for row_delta, col_delta in ((-1, -1), (-1, 1), (1, -1), (1, 1)):
            target_row = row + row_delta
            target_col = col + col_delta
            if is_in_palace(side, target_row, target_col) and can_land(
                board, side, target_row, target_col
            ):
                yield Move(row, col, target_row, target_col)
        return

    if kind == "K":
        for row_delta, col_delta in ORTHOGONAL_DIRECTIONS:
            target_row = row + row_delta
            target_col = col + col_delta
            if is_in_palace(side, target_row, target_col) and can_land(
                board, side, target_row, target_col
            ):
                yield Move(row, col, target_row, target_col)

        step = -1 if side == "red" else 1
        target_row = row + step
        while in_bounds(target_row, col):
            target = board[target_row][col]
            if target is not None:
                if target.side != side and target.kind == "K":
                    yield Move(row, col, target_row, col)
                break
            target_row += step
        return

    if kind == "S":
        forward = -1 if side == "red" else 1
        steps = [(forward, 0)]
        if has_crossed_river(side, row):
            steps.extend(((0, -1), (0, 1)))
        for row_delta, col_delta in steps:
            target_row = row + row_delta
            target_col = col + col_delta
            if can_land(board, side, target_row, target_col):
                yield Move(row, col, target_row, target_col)


def count_between(
    board: Board, from_row: int, from_col: int, to_row: int, to_col: int
) -> int:
    if from_row == to_row:
        start, end = sorted((from_col, to_col))
        return sum(board[from_row][col] is not None for col in range(start + 1, end))
    if from_col == to_col:
        start, end = sorted((from_row, to_row))
        return sum(board[row][from_col] is not None for row in range(start + 1, end))
    return -1


def attacks_square(
    board: Board, row: int, col: int, target_row: int, target_col: int
) -> bool:
    piece = board[row][col]
    if piece is None:
        return False

    row_delta = target_row - row
    col_delta = target_col - col
    abs_row = abs(row_delta)
    abs_col = abs(col_delta)
    kind = piece.kind

    if kind == "R":
        return count_between(board, row, col, target_row, target_col) == 0
    if kind == "C":
        return count_between(board, row, col, target_row, target_col) == 1
    if kind == "H":
        if (abs_row, abs_col) == (2, 1):
            return board[row + row_delta // 2][col] is None
        if (abs_row, abs_col) == (1, 2):
            return board[row][col + col_delta // 2] is None
        return False
    if kind == "E":
        return (
            abs_row == 2
            and abs_col == 2
            and is_on_own_side(piece.side, target_row)
            and board[row + row_delta // 2][col + col_delta // 2] is None
        )
    if kind == "A":
        return abs_row == 1 and abs_col == 1 and is_in_palace(
            piece.side, target_row, target_col
        )
    if kind == "K":
        if col == target_col:
            target = board[target_row][target_col]
            if target and target.kind == "K" and target.side != piece.side:
                return count_between(board, row, col, target_row, target_col) == 0
        return abs_row + abs_col == 1 and is_in_palace(
            piece.side, target_row, target_col
        )
    if kind == "S":
        forward = -1 if piece.side == "red" else 1
        if row_delta == forward and col_delta == 0:
            return True
        return has_crossed_river(piece.side, row) and row_delta == 0 and abs_col == 1
    return False


def is_in_check(board: Board, side: str) -> bool:
    general = find_general(board, side)
    if general is None:
        return True
    general_row, general_col = general
    enemy = opponent(side)
    for row in range(10):
        for col in range(9):
            piece = board[row][col]
            if piece and piece.side == enemy and attacks_square(
                board, row, col, general_row, general_col
            ):
                return True
    return False


def apply_move(board: Board, move: Move) -> Piece | None:
    captured = board[move.to_row][move.to_col]
    board[move.to_row][move.to_col] = board[move.from_row][move.from_col]
    board[move.from_row][move.from_col] = None
    return captured


def generate_legal_moves(board: Board, side: str) -> list[Move]:
    legal_moves: list[Move] = []
    for row in range(10):
        for col in range(9):
            piece = board[row][col]
            if piece is None or piece.side != side:
                continue
            for move in candidate_moves(board, row, col):
                next_board = clone_board(board)
                apply_move(next_board, move)
                if not is_in_check(next_board, side):
                    legal_moves.append(move)
    return legal_moves
