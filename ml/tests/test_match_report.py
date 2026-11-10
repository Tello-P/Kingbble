import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("match_report", ROOT / "scripts" / "match_report.py")
report = importlib.util.module_from_spec(SPEC)
sys.modules["match_report"] = report
SPEC.loader.exec_module(report)


def test_first_score_of_a_seat_line_is_seat_one():
    a_second, a_first = report.parse(
        [
            "game 1: 400 - 450, a in seat 2, a 10 moves 1.00 s, b 10 moves 1.00 s",
            "game 2: 480 - 420, a in seat 1, a 10 moves 1.00 s, b 10 moves 1.00 s",
        ],
        "s",
    )
    assert (a_second.ours, a_second.theirs, a_second.first) == (450, 400, False)
    assert (a_first.ours, a_first.theirs, a_first.first) == (480, 420, True)


def test_named_lines_keep_who_moved_first():
    first, second = report.parse(
        [
            "game 1: kingbble 500 - quackle 450 (kingbble first)",
            "game 2: kingbble 410 - quackle 450",
        ],
        "s",
    )
    assert first.first and first.points == 1
    assert not second.first and second.points == 0


def test_a_game_written_twice_counts_once():
    assert len(report.parse(["game 1: 400 - 450, a in seat 1"] * 2, "s")) == 1


def test_pairs_only_join_games_of_the_same_shard():
    games = report.parse(["game 1: 500 - 400, a in seat 1"], "a") + report.parse(
        ["game 2: 500 - 400, a in seat 2"], "b"
    )
    assert report.pair_points(games) == [1.0, 0.0]


def test_pairs_average_both_seats():
    games = report.parse(["game 1: 500 - 400, a in seat 1", "game 2: 500 - 400, a in seat 1"], "s")
    assert report.pair_points(games) == [1.0, 1.0]  # same seat twice: not a pair
    games = report.parse(["game 1: 500 - 400, a in seat 1", "game 2: 500 - 400, a in seat 2"], "s")
    assert report.pair_points(games) == [0.5]


def test_sprt_accepts_each_side():
    won = report.parse([f"game {g}: kingbble 500 - quackle 400" for g in range(1, 2001)], "s")
    lost = report.parse([f"game {g}: kingbble 400 - quackle 500" for g in range(1, 2001)], "s")
    assert "accepts 53%" in report.sprt(won, 0.5, 0.53, 0.05, 0.05)
    assert "accepts 50%" in report.sprt(lost, 0.5, 0.53, 0.05, 0.05)
    assert "undecided" in report.sprt(won[:3], 0.5, 0.53, 0.05, 0.05)


def test_named_games_are_only_paired_when_asked():
    games = report.parse(
        [
            "game 1: kingbble 500 - quackle 400 (kingbble first)",
            "game 2: kingbble 400 - quackle 500",
        ],
        "s",
    )
    assert report.pair_points(games) == [1.0, 0.0]
    assert report.pair_points(games, named_paired=True) == [0.5]


def test_stats_are_split_by_side_and_phase(tmp_path):
    (tmp_path / "stats.txt").write_text(
        "1 0 50 4.0 100\n1 1 50 2.0 300\n1 0 3 1.0 0\n1 0 0 0.5 0\n"
    )
    rows = report.stats_table([tmp_path])
    assert "kingbble midgame: 1 moves, 4.00 s/move, 100 iterations/move" in rows
    assert "rival midgame: 1 moves, 2.00 s/move, 300 iterations/move" in rows
    assert any(r.startswith("kingbble pre-endgame") for r in rows)
    assert any(r.startswith("kingbble endgame") for r in rows)


def test_stats_skip_a_line_cut_by_a_stopped_run(tmp_path):
    (tmp_path / "stats.txt").write_text("1 0 50 4.0 100\n1 1 50")
    assert report.stats_table([tmp_path]) == [
        "kingbble midgame: 1 moves, 4.00 s/move, 100 iterations/move"
    ]
