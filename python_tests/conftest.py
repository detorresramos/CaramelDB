import carameldb
import numpy as np

gen_str_keys = lambda n: [f"key{i}" for i in range(n)]
gen_byte_keys = lambda n: [f"key{i}".encode("utf-8") for i in range(n)]
gen_int_values = lambda n: np.array([i for i in range(n)])
gen_charX_values = lambda n, x: np.array([str(f"v{i}".ljust(x)[:x]) for i in range(n)])
gen_str_values = lambda n: np.array([f"value{i}" for i in range(n)])


def assert_all_correct(keys, values, csf):
    for key, value in zip(keys, values):
        assert csf.query(key) == value


def assert_build_save_load_correct(keys, values, CSFClass, tmp_path, wrap_fn=None):
    csf = CSFClass(keys, values)
    if wrap_fn:
        csf = wrap_fn(csf)
    assert_all_correct(keys, values, csf)
    filename = str(tmp_path / "temp.csf")
    csf.save(filename)
    csf = CSFClass.load(filename)
    if wrap_fn:
        csf = wrap_fn(csf)
    assert_all_correct(keys, values, csf)


def assert_simple_api_correct(keys, values, tmp_path, prefilter=None):
    csf = carameldb.Caramel(keys, values, prefilter=prefilter)
    assert_all_correct(keys, values, csf)
    filename = str(tmp_path / "temp.csf")
    csf.save(filename)
    csf = carameldb.load(filename)
    assert_all_correct(keys, values, csf)
