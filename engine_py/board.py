from __future__ import annotations

from dataclasses import dataclass


ROWS = 10
COLS = 9
EMPTY = "."


@dataclass(frozen=True)
class Piece:
    code: str

    @property
    def side(self) -> str:
        return "red" if self.code.isupper() else "black"

    @property
    def kind(self) -> str:
        return self.code.upper()


Board = list[list[Piece | None]]


def initial_board() -> Board:
    return board_from_string(
        "rheakaehr/........./.c.....c./s.s.s.s.s/........./"
        "........./S.S.S.S.S/.C.....C./........./RHEAKAEHR"
    )


def clone_board(board: Board) -> Board:
    return [row.copy() for row in board]


def board_from_string(position: str) -> Board:
    rows = position.strip().split("/")
    if len(rows) != ROWS:
        raise ValueError("a position must contain 10 rows")

    board: Board = []
    valid_codes = set("KAEHRCSkaehrcs")
    for row in rows:
        if len(row) != COLS:
            raise ValueError("each position row must contain 9 cells")
        parsed_row: list[Piece | None] = []
        for code in row:
            if code == EMPTY:
                parsed_row.append(None)
            elif code in valid_codes:
                parsed_row.append(Piece(code))
            else:
                raise ValueError(f"invalid piece code: {code}")
        board.append(parsed_row)
    return board


def board_to_string(board: Board) -> str:
    return "/".join(
        "".join(piece.code if piece else EMPTY for piece in row)
        for row in board
    )


def in_bounds(row: int, col: int) -> bool:
    return 0 <= row < ROWS and 0 <= col < COLS


def opponent(side: str) -> str:
    return "black" if side == "red" else "red"


def find_general(board: Board, side: str) -> tuple[int, int] | None:
    wanted = "K" if side == "red" else "k"
    for row in range(ROWS):
        for col in range(COLS):
            piece = board[row][col]
            if piece and piece.code == wanted:
                return row, col
    return None
