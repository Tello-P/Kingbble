from collections import Counter

from ml.data.split import split_of


def test_split_is_deterministic_and_roughly_proportional():
    ids = [f"p{i:016x}" for i in range(10_000)]
    splits = [split_of(i) for i in ids]
    assert splits == [split_of(i) for i in ids]
    counts = Counter(splits)
    assert 7_700 < counts["train"] < 8_300
    assert 800 < counts["val"] < 1_200 and 800 < counts["test"] < 1_200
