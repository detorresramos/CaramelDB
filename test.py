import carameldb

csf = carameldb.PyCsfUint32.construct([b"1", b"2", b"3"], [1, 2, 3])
print(csf.query(b"1"))
csf.save(b"file.csf")
csf2 = carameldb.PyCsfUint32.load(b"file.csf")
print(csf2.query(b"1"))

import os

os.remove("file.csf")


import carameldb

csf = carameldb.PyMultisetCsfInt.construct(
    [b"1", b"2", b"3"], [[1, 2, 3], [2, 3, 4], [3, 4, 5], [4, 5, 6]]
)
print(csf.query(b"1", False))
csf.save(b"file.csf")
csf2 = carameldb.PyMultisetCsfInt.load(b"file.csf")
print(csf2.query(b"1", False))

import os

os.remove("file.csf")
