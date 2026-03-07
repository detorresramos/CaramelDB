import carameldb
import numpy as np
from carameldb import AutoFilterConfig
from conftest import assert_all_correct, gen_str_keys


def test_auto_filter_skewed_data():
    """AutoFilterConfig should select a filter for highly skewed data."""
    rows = 10000
    keys = gen_str_keys(rows)
    values = [42 for _ in range(int(rows * 0.9))] + list(range(int(rows * 0.1)))

    csf = carameldb.Caramel(keys, values, prefilter=AutoFilterConfig(), verbose=False)
    assert_all_correct(keys, values, csf)
    assert csf.get_filter() is not None


def test_auto_filter_uniform_data():
    """AutoFilterConfig should skip filter for uniform data."""
    rows = 1000
    keys = gen_str_keys(rows)
    values = list(range(rows))

    csf = carameldb.Caramel(keys, values, prefilter=AutoFilterConfig(), verbose=False)
    assert_all_correct(keys, values, csf)
    assert csf.get_filter() is None


def test_auto_filter_all_identical():
    """AutoFilterConfig should skip filter when all values are identical (alpha=1)."""
    keys = gen_str_keys(1000)
    values = [7] * 1000

    csf = carameldb.Caramel(keys, values, prefilter=AutoFilterConfig(), verbose=False)
    assert_all_correct(keys, values, csf)
    assert csf.get_filter() is None


def test_auto_filter_save_load(tmp_path):
    """AutoFilterConfig CSFs should round-trip through save/load."""
    rows = 5000
    keys = gen_str_keys(rows)
    values = [42 for _ in range(int(rows * 0.9))] + list(range(int(rows * 0.1)))

    csf = carameldb.Caramel(keys, values, prefilter=AutoFilterConfig(), verbose=False)
    assert_all_correct(keys, values, csf)

    filename = str(tmp_path / "auto.csf")
    csf.save(filename)
    loaded = carameldb.load(filename)
    assert_all_correct(keys, values, loaded)
