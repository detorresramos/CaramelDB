import time

import caramel

num = 10_000_000

keys = [f"key{i}" for i in range(num)]
values = [i for i in range(num)]

start = time.time()
csf = caramel.CSF(keys, values)
end_construction = time.time()
print("Total Construction Time: ", end_construction - start)

for key, value in zip(keys, values):
    assert csf.query(key) == value
