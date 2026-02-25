import json
import os
import shutil
import subprocess
import sys
import tempfile
import warnings

import carameldb
import numpy as np

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

import theory
from data_gen import compute_actual_alpha
from measure import create_filter_config
from shibuya import empirical_entropy, shibuya_bloom_params


def _hash_table_memory(keys, values):
    return {"theoretical": sum(len(k) for k in keys) + len(keys) * 4}


def _csf_stats_to_dict(stats):
    d = {
        "in_memory_bytes": stats.in_memory_bytes,
        "solution_bytes": stats.solution_bytes,
        "filter_bytes": stats.filter_bytes,
        "metadata_bytes": stats.metadata_bytes,
    }
    bs = stats.bucket_stats
    d["bucket_stats"] = {
        "num_buckets": bs.num_buckets,
        "total_solution_bits": bs.total_solution_bits,
        "avg_solution_bits": bs.avg_solution_bits,
        "min_solution_bits": bs.min_solution_bits,
        "max_solution_bits": bs.max_solution_bits,
    }
    hs = stats.huffman_stats
    d["huffman_stats"] = {
        "num_unique_symbols": hs.num_unique_symbols,
        "max_code_length": hs.max_code_length,
        "avg_bits_per_symbol": hs.avg_bits_per_symbol,
        "code_length_distribution": list(hs.code_length_distribution),
    }
    fs = stats.filter_stats
    if fs is not None:
        d["filter_stats"] = {
            "type": fs.type,
            "size_bytes": fs.size_bytes,
            "num_elements": fs.num_elements,
            "num_hashes": fs.num_hashes,
            "size_bits": fs.size_bits,
            "fingerprint_bits": fs.fingerprint_bits,
        }
    else:
        d["filter_stats"] = None
    return d


class HashTable:
    name = "hash_table"

    @staticmethod
    def construct(keys, values):
        return {k: int(v) for k, v in zip(keys, values)}

    @staticmethod
    def query(structure, key):
        return structure[key]

    measure_memory = staticmethod(_hash_table_memory)

    @staticmethod
    def get_params():
        return None


class CppHashTable:
    name = "cpp_hash_table"

    @staticmethod
    def construct(keys, values):
        return carameldb.UnorderedMapBaseline(keys, np.array(values, dtype=np.uint32))

    @staticmethod
    def query(structure, key):
        return structure.query(key)

    measure_memory = staticmethod(_hash_table_memory)

    @staticmethod
    def get_params():
        return None


EPSILON_STRATEGIES = ("optimal", "shibuya")


def _find_optimal_params(filter_type, keys, values):
    n = len(keys)
    alpha = compute_actual_alpha(values)
    n_over_N = len(np.unique(values)) / n
    n_filter = int(n * (1 - alpha))

    if filter_type == "xor":
        bits, _ = theory.best_discrete_xor(alpha, n_over_N)
        return {"fingerprint_bits": bits}
    elif filter_type == "binary_fuse":
        bits, _ = theory.best_discrete_binary_fuse(alpha, n_over_N, n_filter)
        return {"fingerprint_bits": bits}
    elif filter_type == "bloom":
        best_lb, best_bpe, best_k = float("-inf"), 1, 1
        for k in range(1, 9):
            bpe, lb = theory.best_discrete_bloom(alpha, n_over_N, k)
            if lb > best_lb:
                best_lb, best_bpe, best_k = lb, bpe, k
        return {"bloom_bits_per_element": best_bpe, "bloom_num_hashes": best_k}
    else:
        raise ValueError(f"Unknown filter type: {filter_type}")


def _find_shibuya_params(keys, values):
    alpha = compute_actual_alpha(values)
    H0 = empirical_entropy(values)
    result = shibuya_bloom_params(alpha, H0)
    if result is None:
        bits_per_element, num_hashes = 1, 1
    else:
        bits_per_element, num_hashes = result
    return {"bloom_bits_per_element": bits_per_element, "bloom_num_hashes": num_hashes}


class CSFFilter:
    def __init__(self, filter_type="binary_fuse", epsilon_strategy="optimal"):
        if epsilon_strategy not in EPSILON_STRATEGIES:
            raise ValueError(
                f"Unknown epsilon_strategy: {epsilon_strategy}, "
                f"must be one of {EPSILON_STRATEGIES}"
            )
        self.epsilon_strategy = epsilon_strategy
        if epsilon_strategy == "shibuya":
            self.filter_type = "bloom"
        else:
            self.filter_type = filter_type
        self.name = f"csf_filter_{epsilon_strategy}_{self.filter_type}"
        self._params = None

    def construct(self, keys, values):
        if self.epsilon_strategy == "optimal":
            self._params = _find_optimal_params(self.filter_type, keys, values)
        else:
            self._params = _find_shibuya_params(keys, values)
        config = create_filter_config(self.filter_type, **self._params)
        return carameldb.Caramel(keys, values, prefilter=config, verbose=False)

    @staticmethod
    def query(structure, key):
        return structure.query(key)

    @staticmethod
    def measure_memory(keys, values):
        return None

    def measure_memory_from_structure(self, structure):
        stats = structure.get_stats()
        with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as f:
            tmp_path = f.name
        try:
            structure.save(tmp_path)
            serialized_bytes = os.path.getsize(tmp_path)
        finally:
            os.unlink(tmp_path)
        return {
            "serialized": serialized_bytes,
            "csf_stats": _csf_stats_to_dict(stats),
        }

    def get_params(self):
        return self._params


CARAMEL_JAVA_DIR = os.path.normpath(
    os.path.join(_dir, "..", "..", "..", "caramel-java")
)
JAVA_CSF_JAR = os.path.join(
    CARAMEL_JAVA_DIR, "java-caramel", "target", "java-caramel-1.0-SNAPSHOT.jar"
)
JAVA_MPH_JAR = os.path.join(
    CARAMEL_JAVA_DIR, "java-mph", "target", "mph-table-1.0.6-SNAPSHOT.jar"
)


def _find_java():
    homebrew_java = "/opt/homebrew/opt/openjdk/bin/java"
    if os.path.isfile(homebrew_java):
        return homebrew_java
    return shutil.which("java")


def _run_java_benchmark(jar_path, main_class, keys, values, seed, num_queries=100):
    java_bin = _find_java()
    if java_bin is None:
        warnings.warn("java not found on PATH, skipping Java benchmark")
        return None
    if not os.path.isfile(jar_path):
        warnings.warn(f"JAR not found: {jar_path}, skipping Java benchmark")
        return None

    tmp_paths = []
    try:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            tmp_paths.append(f.name)
            f.write("\n".join(keys))

        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            tmp_paths.append(f.name)
            f.write(np.array(values, dtype=">u8").tobytes())

        tmp_keys, tmp_values = tmp_paths
        cmd = [
            java_bin,
            "-Dorg.slf4j.simpleLogger.logFile=System.err",
            "--add-opens=java.base/java.io=ALL-UNNAMED",
            "--add-opens=java.base/java.nio=ALL-UNNAMED",
            "--enable-native-access=ALL-UNNAMED",
            "-Xmx4g",
            "-cp",
            jar_path,
            main_class,
            tmp_keys,
            tmp_values,
            str(seed),
            str(num_queries),
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        if result.returncode != 0:
            warnings.warn(
                f"Java benchmark failed (rc={result.returncode}):\n{result.stderr}"
            )
            return None
        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        warnings.warn("Java benchmark timed out after 600s")
        return None
    except Exception as e:
        warnings.warn(f"Java benchmark error: {e}")
        return None
    finally:
        for path in tmp_paths:
            if os.path.exists(path):
                os.unlink(path)


class JavaBenchmark:
    def __init__(self, name, jar_path, main_class, params=None):
        self.name = name
        self._jar_path = jar_path
        self._main_class = main_class
        self._params = params

    def run_full_benchmark(self, keys, values, seed):
        return _run_java_benchmark(self._jar_path, self._main_class, keys, values, seed)

    def get_params(self):
        return self._params


def JavaCSF():
    return JavaBenchmark(
        "java_csf", JAVA_CSF_JAR, "com.randorithms.app.Benchmark", {"codec": "Huffman"}
    )


def JavaMPH():
    return JavaBenchmark("java_mph", JAVA_MPH_JAR, "com.indeed.mph.Benchmark")
