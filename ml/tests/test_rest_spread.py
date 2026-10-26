import math

import pytest

from ml.rl.rest_spread import by_bag, final_scores, fit, residuals

GCG = """#player1 a A
#player2 b B
>a: ABC 8D AB +10 10
>b: DEF 8F .EF +12 12
>a: CGH 9D C +5 15
>b: (time) -10 2
"""


def test_final_scores_take_each_players_last_cumulative():
    assert final_scores(GCG) == [15, 2]


def test_residuals_compare_the_final_margin_with_the_margin_plies_later():
    # (player, score_before, opp_score_before, bag_before); final 50 - 30.
    rows = [(0, 0, 0, 86), (1, 10, 0, 80), (0, 0, 10, 75), (1, 10, 20, 70), (0, 40, 30, 65)]
    out = residuals(rows, [50, 30], plies=3)
    # Turn 0 (player 0): margin at turn 3 is 20 - 10 = 10; final 20.
    assert out[0] == (86, 10)
    # Turn 1 (player 1): margin at turn 4 is 30 - 40 = -10; final -20.
    assert out[1] == (80, -10)
    # Turns 2-4 end within the playout: nothing left to come.
    assert out[2:] == [(75, 0), (70, 0), (65, 0)]


def test_fit_recovers_sd_linear_in_the_root_of_the_bag():
    points = [(bag, 4 + 12 * math.sqrt(bag), 10) for bag in range(1, 60, 5)]
    sd0, per_sqrt = fit(points)
    assert sd0 == pytest.approx(4)
    assert per_sqrt == pytest.approx(12)


def test_by_bag_averages_squared_residuals():
    assert by_bag([(5, 3), (5, -1), (7, 2)]) == {5: (5.0, 2), 7: (4.0, 1)}
