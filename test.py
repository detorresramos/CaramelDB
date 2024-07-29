import carameldb

csf = carameldb.PyCsfInt.construct([b"1", b"2", b"3"], [1, 2, 3])
print(csf.query(b"1"))
csf.save(b"file.csf")
csf2 = carameldb.PyCsfInt.load(b"file.csf")
print(csf2.query(b"1"))

import os
os.remove("file.csf")