import caramel
import time

num = 100_000_000

keys = [f"key{i}" for i in range(num)]
values = []

value = 0
num_added = 0
power_of_2 = int(num / 2)
for i in range(num):
    values.append(value)
    if num_added == power_of_2:
        power_of_2 = int(power_of_2 / 2)
        value += 1
        num_added = 0
    else:
        num_added += 1

start = time.time()
csf = caramel.CSF(keys, values)
end_construction = time.time()
print("Total Construction Time: ", end_construction - start)

# for key, value in zip(keys, values):
#     csf.query(key)