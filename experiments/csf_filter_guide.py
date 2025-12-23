import os
import tempfile
import carameldb
from carameldb import BloomFilterConfig, XORFilterConfig, BinaryFuseFilterConfig


def measure_size(csf):
    with tempfile.NamedTemporaryFile(suffix=".csf", delete=False) as f:
        path = f.name
    try:
        csf.save(path)
        return os.path.getsize(path)
    finally:
        os.remove(path)


def print_stats(csf, name):
    stats = csf.get_stats()
    sz = measure_size(csf)
    print(f"\n{name}")
    print(f"  serialized: {sz:,} bytes")
    print(f"  solution: {stats.solution_bytes:.0f}, filter: {stats.filter_bytes:.0f}, meta: {stats.metadata_bytes:.0f}")
    if stats.filter_stats:
        fs = stats.filter_stats
        extra = f"hashes={fs.num_hashes}" if fs.num_hashes else f"fp_bits={fs.fingerprint_bits}"
        print(f"  filter: {fs.type}, {fs.size_bytes:,} bytes, {extra}")
    return sz


N = 100_000
ALPHA = 0.9

keys = [f"key_{i:08d}" for i in range(N)]
n_common = int(N * ALPHA)
values = [999999] * n_common + list(range(N - n_common))

print(f"N={N:,}, alpha={ALPHA} ({n_common:,} common, {N - n_common:,} uncommon)\n")

csf = carameldb.Caramel(keys, values, prefilter=None, verbose=False)
sz_none = print_stats(csf, "No Filter")

bloom = BloomFilterConfig(bits_per_element=10, num_hashes=7)
csf = carameldb.Caramel(keys, values, prefilter=bloom, verbose=False)
print_stats(csf, "Bloom(bits_per_element=10, num_hashes=7)")

xor = XORFilterConfig(fingerprint_bits=8)
csf = carameldb.Caramel(keys, values, prefilter=xor, verbose=False)
print_stats(csf, "XOR(fingerprint_bits=8)")

bf = BinaryFuseFilterConfig(fingerprint_bits=8)
csf = carameldb.Caramel(keys, values, prefilter=bf, verbose=False)
print_stats(csf, "BinaryFuse(fingerprint_bits=8)")

