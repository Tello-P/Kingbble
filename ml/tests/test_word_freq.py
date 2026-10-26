from ml.data.word_freq import read_words, table, write


def test_table_keeps_words_with_a_frequency_or_plays(tmp_path):
    lex = tmp_path / "words.txt"
    lex.write_text("qi\nZZZS extra\nRETAINS\nqi\n")
    words = read_words(lex)
    assert words == ["QI", "RETAINS", "ZZZS"]
    rows = table(words, {"ZZZS": 2}, lambda w: {"QI": 3.3}.get(w, 0.0))
    assert rows == [("QI", 3.3, 0), ("ZZZS", 0.0, 2)]
    out = tmp_path / "f.tsv"
    write(rows, out)
    assert out.read_text().splitlines() == ["word\tzipf\tplays", "QI\t3.30\t0", "ZZZS\t0.00\t2"]
