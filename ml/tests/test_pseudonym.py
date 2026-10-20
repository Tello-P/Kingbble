from ml.data.pseudonym import load_salt, pseudonym


def test_pseudonym_is_stable_and_salted():
    a = pseudonym("ann", "s1")
    assert a == pseudonym("Ann", "s1")
    assert a != pseudonym("bob", "s1") and a != pseudonym("ann", "s2")
    assert a.startswith("p") and len(a) == 17 and "ann" not in a


def test_load_salt_creates_a_secret_file_once(tmp_path, monkeypatch):
    monkeypatch.delenv("KINGBBLE_SALT", raising=False)
    path = tmp_path / "salt"
    salt = load_salt(path)
    assert len(salt) == 64 and load_salt(path) == salt
    assert path.stat().st_mode & 0o077 == 0
    monkeypatch.setenv("KINGBBLE_SALT", "env")
    assert load_salt(path) == "env"
