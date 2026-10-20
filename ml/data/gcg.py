"""GCG game records (as written by Woogles and Quackle) and their turns.

A turn is one move event (placement, exchange or pass) with the rack the player
held before it. Racks are what makes a turn usable for the human model: the
record of a turn without a rack only says what was played.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

_COORD = re.compile(r"^(\d{1,2}[A-O]|[A-O]\d{1,2})$")
_RACK = re.compile(r"^[A-Za-z?]{1,7}$")
_SCORE = re.compile(r"^[+-]?\d+$")

TOTAL_TILES = 100
RACK_SIZE = 7


@dataclass
class Event:
    """One move line of a GCG file."""

    kind: str  # place, exchange, pass, phony, challenge, end_points, end_penalty, time
    nickname: str
    rack: str  # "" when unknown
    position: str = ""  # placements: "8D" (horizontal) or "D8" (vertical)
    word: str = ""  # placements: as written, '.' or (X) for played-through tiles
    exchanged: str = ""  # exchanges: the tiles, or a count when they are unknown
    score: int = 0
    cumulative: int = 0
    line: int = 0


@dataclass
class GcgGame:
    players: list[tuple[str, str]] = field(default_factory=list)  # (nickname, name)
    lexicon: str = ""
    game_id: str = ""
    events: list[Event] = field(default_factory=list)


class GcgError(ValueError):
    pass


def new_tiles(word: str) -> int:
    """Tiles a placement puts on the board: letters outside parentheses, not '.'."""
    return len(re.sub(r"\([^)]*\)", "", word).replace(".", ""))


def parse_gcg(text: str) -> GcgGame:
    game = GcgGame()
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if line.startswith("#"):
            key, _, value = line[1:].partition(" ")
            value = value.strip()
            if key in ("player1", "player2"):
                nickname, _, name = value.partition(" ")
                game.players.append((nickname, name.strip()))
            elif key == "lexicon":
                game.lexicon = value
            elif key == "id":
                game.game_id = value
            continue
        if not line.startswith(">"):
            continue  # note continuations and blank lines
        game.events.append(_parse_event(line, number))
    if len(game.players) != 2:
        raise GcgError("a GCG game needs #player1 and #player2")
    return game


def _parse_event(line: str, number: int) -> Event:
    nickname, sep, rest = line[1:].partition(":")
    tokens = rest.split()
    if not sep or len(tokens) < 2 or not _SCORE.match(tokens[-1]) or not _SCORE.match(tokens[-2]):
        raise GcgError(f"line {number}: cannot read '{line}'")
    middle = tokens[:-2]
    ev = Event("pass", nickname.strip(), "", score=int(tokens[-2]), cumulative=int(tokens[-1]))
    ev.line = number
    if not middle:
        raise GcgError(f"line {number}: no move in '{line}'")
    if len(middle) == 1 and middle[0].startswith("("):
        ev.kind, ev.rack = "end_points", middle[0].strip("()")
        return ev
    if middle[0] and _RACK.match(middle[0]) and not _COORD.match(middle[0]):
        ev.rack = middle[0]
        middle = middle[1:]
    if not middle:
        raise GcgError(f"line {number}: no move in '{line}'")
    move = middle[0]
    if move == "--":
        ev.kind = "phony"
    elif move == "(challenge)":
        ev.kind = "challenge"
    elif move == "(time)":
        ev.kind = "time"
    elif move.startswith("("):
        ev.kind = "end_penalty"
    elif move == "-":
        ev.kind = "pass"
    elif move.startswith("-"):
        ev.kind, ev.exchanged = "exchange", move[1:]
    elif _COORD.match(move) and len(middle) == 2:
        ev.kind, ev.position, ev.word = "place", move, middle[1]
    else:
        raise GcgError(f"line {number}: unknown move in '{line}'")
    return ev


def turns(game: GcgGame) -> list[dict]:
    """One row per placement, exchange or pass, in order.

    Each row has the mover's rack before the move ("" if unknown), the scores of
    both players before it and the tiles left in the bag. Placements later
    withdrawn as phonies are kept and flagged.
    """
    seat = {nick: i for i, (nick, _) in enumerate(game.players)}
    scores = [0, 0]
    bag = TOTAL_TILES - 2 * RACK_SIZE
    rows: list[dict] = []
    drawn: list[int] = []  # tiles drawn by each placement, to undo phonies
    for ev in game.events:
        if ev.nickname not in seat:
            raise GcgError(f"line {ev.line}: unknown player '{ev.nickname}'")
        p = seat[ev.nickname]
        if ev.kind in ("place", "exchange", "pass"):
            rows.append(
                {
                    "turn": len(rows),
                    "player": p,
                    "nickname": ev.nickname,
                    "rack": ev.rack.upper(),
                    "kind": ev.kind,
                    "position": ev.position,
                    "word": ev.word,
                    "exchanged": ev.exchanged,
                    "score": ev.score,
                    "score_before": scores[p],
                    "opp_score_before": scores[1 - p],
                    "bag_before": bag,
                    "phony": False,
                }
            )
        if ev.kind == "place":
            take = min(new_tiles(ev.word), bag)
            bag -= take
            drawn.append(take)
        elif ev.kind == "phony":
            if not rows or rows[-1]["kind"] != "place" or not drawn:
                raise GcgError(f"line {ev.line}: phony withdrawn after no placement")
            rows[-1]["phony"] = True
            bag += drawn.pop()
        scores[p] = ev.cumulative
    return rows
