import caramel
import time

keys = [f"key{i}" for i in range(1000000)]
values = [i for i in range(1000000)]

start = time.time()
csf = caramel.CSF(keys, values)
end_construction = time.time()
print("Total Construction Time: ", end_construction - start)

for key, value in zip(keys, values):
    assert csf.query(key) == value