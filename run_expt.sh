cython/.venv/bin/python3 scripts/benchmark_multiset.py --npy ~/recsys-datasets/amazon_books_100m.npy --permutation none --shared-codebook --save-dir results/amazon_books_100m_nopermute_sharedcb --output-json results/amazon_books_100m_nopermute_sharedcb.json

cython/.venv/bin/python3 scripts/benchmark_multiset.py --npy ~/recsys-datasets/amazon_books_50m.npy --permutation none --shared-codebook --save-dir results/amazon_books_50m_nopermute_sharedcb --output-json results/amazon_books_50m_nopermute_sharedcb.json

cython/.venv/bin/python3 scripts/benchmark_multiset.py --npy ~/recsys-datasets/amazon_books_50m.npy --permutation global_sort --save-dir results/amazon_books_50m_permute_nosharedcb --output-json results/amazon_books_50m_permute_nosharedcb.json
