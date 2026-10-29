import pytest

from ml.rl.leave_fit import fit, leave_of, played_tiles, samples

GAME = """#player1 a A
#player2 b B
>a: ABCDEFG 8D BAD +12 12
>b: HIJKLMN -HIJ +0 0
>a: CEFGXYZ 9E FE +10 22
>b: KLMNOPQ 7C KO +8 8
"""


def test_played_tiles_and_leaves():
    assert played_tiles("B.d(E)") == "B?"
    assert leave_of("ABCDEFG", "BAD") == "CEFG"
    assert leave_of("AB?", "?") == "AB"
    assert leave_of("ABC", "Z") is None


def test_samples_credit_the_leave_with_what_follows():
    rows = samples(GAME)
    # Final scores 22 - 8. After a's first move the spread is 12, so +2 follows.
    assert rows[0] == ("CEFG", 2.0)
    # b's exchange keeps KLMN with b at 0 - 12 right after it: -14 - (-12) = -2.
    assert rows[1] == ("KLMN", -2.0)
    assert len(rows) == 4


def test_fit_centres_the_empty_leave_and_shrinks_rare_leaves():
    pytest.importorskip("numpy")
    rows = [("S", 8.0)] * 40 + [("Q", -8.0)] * 40 + [("", 0.0)] * 40 + [("SS", 20.0)]
    values = fit(rows, l2=1.0, shrink=10.0)
    assert values["S"] > 4 > values["Q"] + 8
    assert "" not in values and set("AQZ?") <= set(values)
    lonely = fit(rows, l2=1.0, shrink=1000.0)["SS"]
    assert abs(lonely - values["SS"]) > 0  # more shrinkage moves the rare leave
