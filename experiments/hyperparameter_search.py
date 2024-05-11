import os

for distribution in ["zipfian", "geometric", "uniform"]:
    for num_columns in [1, 10, 100, 1000]:
        for num_rows in [100, 1000, 10000, 100000]:
            print("----------")
            os.system(
                f"python3 csf_benchmark.py --rows {num_rows} --columns {num_columns} --dist {distribution}"
            )
            print("----------")
