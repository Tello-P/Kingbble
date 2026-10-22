import json

from ml.eval.results_log import ResultsLog


def test_records_survive_a_restart_and_a_cut_line(tmp_path):
    path = tmp_path / "r.txt"
    log = ResultsLog(path)
    log.append({"i": 0, "ours": 3})
    log.append({"i": 0, "theirs": 3})
    # A stop in the middle of a write leaves half a line.
    with path.open("a") as f:
        f.write('{"i": 1, "our')
    # A read-only look (status) neither uses nor removes it.
    assert ResultsLog(path, read_only=True).records == log.records
    assert path.read_text().endswith('"our')
    resumed = ResultsLog(path)
    assert resumed.records == [{"i": 0, "ours": 3}, {"i": 0, "theirs": 3}]
    resumed.append({"i": 1, "ours": None})
    lines = path.read_text().splitlines()
    assert [json.loads(line) for line in lines][-1] == {"i": 1, "ours": None}


def test_no_file_keeps_records_in_memory(tmp_path):
    log = ResultsLog(None)
    log.append({"x": 1})
    assert log.records == [{"x": 1}]
    assert ResultsLog(tmp_path / "missing.txt").records == []
