from ml.rl.risk_policy import Score, halving, policy_name, policy_text, score_results


def test_policy_text_and_name():
    text = policy_text([(7, 12, -1e9, -1)], [0.5])
    assert text.splitlines()[1] == "7 12 -1e+09 -1 0.5"
    assert policy_name([1.0, 0.5, 2.0]) == "1_0.5_2"


def test_score_results_counts_bot_a_in_either_seat(tmp_path):
    path = tmp_path / "r.txt"
    path.write_text(
        "game 1: 400 - 350, a in seat 1, a 10 moves 1.0 s, b 9 moves 1.0 s\n"
        "game 2: 400 - 350, a in seat 2, a 9 moves 1.0 s, b 10 moves 1.0 s\n"
        "game 3: 300 - 300, a in seat 1\n"
    )
    s = score_results(path)
    assert (s.games, s.wins, s.spread) == (3, 1.5, 0)
    assert score_results(tmp_path / "missing.txt").games == 0


def test_halving_keeps_the_better_half_and_returns_the_best():
    true_win = {0.0: 0.40, 1.0: 0.50, 2.0: 0.60, 4.0: 0.45}
    calls = []

    def evaluate(arm, total):
        calls.append((arm, total))
        return Score(games=total, wins=true_win[arm] * total, spread=0.0)

    best, scores = halving(list(true_win), evaluate, games=10, rounds=3)
    assert best == 2.0
    assert {a for a, t in calls if t == 20} == {2.0, 1.0}
    assert {a for a, t in calls if t == 40} == {2.0}
    assert scores[2.0].games == 40
