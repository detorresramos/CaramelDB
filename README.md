# CaramelDB: A High-Performance Succinct Retrieval Library

CaramelDB is a performant C++/Python library of succinct retrieval data structures with applications in data management, machine learning systems, computational biology, and more! The centerpiece of CaramelDB is an implementation of the [CARAMEL data structure](https://arxiv.org/pdf/2305.16545.pdf), a space-efficient table with state-of-the-art performance on practical succinct retrieval workloads. 

## What is Succinct Retrieval?

As defined in [Dillinger, et al. (2022)](https://drops.dagstuhl.de/storage/00lipics/lipics-vol233-sea2022/LIPIcs.SEA.2022.4/LIPIcs.SEA.2022.4.pdf), a *retrieval data structure* $D$ for a static function $f \colon S \to \{0, 1\}^r$ is an index that returns $f(x)$ for any $x \in S$ and any value for $x \notin S$. A highly desirable goal in both theory and practice is to represent $D$ with as little space as possible. To that end, a *succinct retrieval data structure* is an index that uses only $(1 + o(1))r|S|$ bits of space, just slightly more than the information-theoretic lower bound of $r|S|$ bits.

In the age of massive datasets and machine learning, we observe that numerous applications may benefit from succinct retrieval structures. For example, many industrial search and recommendation systems services reply on computationally expensive machine learning models to compute results. Since executing these queries in real-time is prohibitively expensive, these services often cache the most frequently issues queries along with their corresponding results for faster and more cost-effective serving. As search traffic volume increases over time, it becomes increasingly desirable to store this cache in as few bits as possible via succinct retrieval methods.

## Succinct Retrieval Methods

Succinct retrieval is a beautiful research area because it lies at the nexus of theory and practice. The computer science community has proposed several solutions to this problem in recent years through a combination of clever algorithmic insights and innovative performance engineering efforts. These developments included Minimal Perfect Hash Tables, Compressed Static Functions, and Bumped Ribbon Retrieval (BuRR). In the near future, we envision including all of these implementations, in addition to our own CARAMEL data structure, within the CaramelDB project as a one-stop shop for succinct retrieval in the Python ecosystem. 

## What is CARAMEL?

CARAMEL (Compressed Array Representation And More Efficient Lookups) is a the centerpiece data structure in CaramelDB, achieving state-of-the-art performance in many practical settings, especially when the values to retrieve are multi-sets and not individual objects. The core algorithmic primitive behind CARAMEL is the compressed static function (CSF). For a solid introduction to CSFs, see the excellent series of papers by Sebastiano Vigna's group. We draw heavy inspiration from their [Java implementation in Sux4J](https://sux4j.di.unimi.it).

## Why CaramelDB?

Compared to Sux4J, we offer about 25% faster construction, a much smaller and simpler codebase (> 10k vs 2k LoC for the core implementation), and easy Python bindings. We support arbitrary data types for keys and values, enabling more flexibility for applications (the original Sux4J implementation supports only string keys and integer values). We also have some algorithmic improvements to make CSF-backed tables practical for real applications. For example, we have optimal-size Bloom prefilters for situations when one value is very common. We also have value-permutation methods to preprocess 2D arrays into a more compressible format, as well as tricks to implement ragged arrays and other lookup primitives using CSFs. Our paper describes these tricks and applications.

### Minimal API Example

You can use our Python API like this.

```python
from carameldb import Caramel
keys = ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]
values = [0, 0, 0, 1, 2, 0, 1, 4, 0, 1]
# Constructing
caramel = Caramel(keys, values)
# Querying
caramel.query("D")  # Returns 1
# Saving
caramel.save("map.caramel")
# Loading
caramel = Caramel.load("map.caramel")
```

## Getting Started
The recommended way to interact with our library is through the Python bindings, though we also offer a C++ API. 

### Python Installation
To install the Python bindings, first clone the repository and all of our dependencies.

```bash
git clone --recurse-submodules git@github.com:detorresramos/caramel.cpp.git
cd caramel.cpp
```

Then, install the required packages using our install scripts: `sh bin/install-linux.sh` or `sh bin/install-mac.sh`.
**Note** The linux install script assumes apt as the package manager. You may need to modify the script if using a different package manager.
Generally, we require g++, cmake, and OpenMP, in addition to pytest for the test suite. 

Next, run our build script:
```bash
python3 bin/build.py
```

You should see a significant amount of CMake build output, (hopefully) ending with a successful installation!
```
... build output ...
  -- Configuring done
  -- Generating done
  -- Build files have been written to: /Users/bencoleman/Documents/code/src/caramel.cpp/build
  [  4%] Built target libomp-needed-headers
  Consolidate compiler generated dependencies of target omp
  [ 74%] Built target omp
  Consolidate compiler generated dependencies of target caramel_lib
  [ 95%] Built target caramel_lib
  Consolidate compiler generated dependencies of target _caramel
  [ 97%] Building CXX object CMakeFiles/_caramel.dir/python_bindings/PythonBindings.cc.o
  [100%] Linking CXX shared module lib.macosx-12-arm64-cpython-311/caramel/_caramel.cpython-311-darwin.so
  [100%] Built target _caramel
... more build output ...
Successfully installed caramel-0.0.1
```

**Tip:** If you use Anaconda or virtual environments to manage your Python environments, be sure to switch to the correct environment before calling `bin/build.py`. Our build script will build and install the Python module into *your current Python environment!*

**Build dependencies:** We require Python >= 3.8 and a C++ compiler supporting C++20. Older versions may build, but we can't guarantee support.

### Testing your installation
To run the Python tests, you can run
```bash
bin/python-test.sh
```
However, you can also play around with CSFs directly, using the simple API examples from earlier.

### Using CARAMEL in Python

Our Python interface is intentionally minimal. We only expose two public methods: `caramel.CSF(keys, values)` to construct a CSF and `caramel.load(filename)` to load a CSF from a file. These methods work with a variety of supported key and value types.

For example, we can pass keys as strings:
```python
from carameldb import Caramel
keys = ["cats", "dogs", "fish"]
values = [2, 1, 0]
csf = Caramel(keys, values)
```

We can also pass keys as byte strings:
```python
keys = [s.encode("utf-8") for s in ("cats", "dogs", "fish")]
values = [2, 1, 0]
caramel = Caramel(keys, values)
```

Note that this permits the use of arbitrary objects as keys.
```python
import pickle
keys = [(1, 2), "abc", 12345, float('inf')]
values = [1, 2, 3, 4]

keys = [pickle.dumps(k) for k in keys]
caramel = Caramel(keys, values)
```

We can also call out to special, more efficient / optimized routines if the value strings are all of the same (known) length. Length-10 and length-12 are explicitly supported, but this is very easy to extend.
```python
# For example, Amazon product lookups.
keys = ["Product A", "Product B", "Product C"]
asins = ["B0775MV9K2", "B0816J62HY", "B008C2BTTO"]
csf = caramel.CSF(keys, asins)
# Or genomics string-to-string maps.
keys = ["kmer-a", "kmer-b", "kmer-c"]
values = ["ATCGAATACC", "TAGCCTAGCA", "CAATGAGTAA"]
csf = caramel.CSF(keys, values)
```

### Using CARAMEL in C++
Our C++ API is present in `src/construct/Csf.h`. We do not currently have usage examples - but we do have a large library of tests that show the intended use of the API.

At a high level, the C++ API contains several templated backends for different data types for the keys and values. Our Python module automatically infers which backend to use and wraps the full process of choosing a backend. The C++ header exposes a much greater level of detail, allowing you to define new backends using our `Csf<T>` template class.

## Development

### Contributing
We welcome issues, bug reports, and contributions from the community. If there's a feature you'd like to request, fill out an issue or make a PR.

### Development Scripts
We use the following scripts to manage formatting, linting, building, and testing.
* `bin/build.py` is the main installation script which builds and installs the python package by default. 
* Running `bin/build.py` with `-t` specifies the target to build, for example `bin/build.py -t all` will build everything, including the C++ tests. When a target is specified, `bin/build.py` does not install the python package.
* To run all the python tests run `bin/python-test.sh` after `bin/build.py`. To run a specific test run `bin/python-test.sh -k <name_of_the_test_function_or_file>`.
* To run all the C++ tests run `bin/cpp-test.sh` after `bin/build.py -t all`. To run a specific test run `bin/cpp-test.sh -R <NameOfTheTestFixture>`.
*  Run `$ bin/cpp-format.sh` from anywhere to format all C++ code.
*  Run `$ bin/python-format.sh` from anywhere to format all Python code.
*  Run `$ bin/generate_compile_commands.sh` from anywhere to generate the compile
commands database (you shouldn't often need to do this manually, but try it
if your intellisense is acting strangely).

## Citation

If you use our library, please cite our preprint:
```
@article{ramos2023caramel,
  title={CARAMEL: A Scalable Index for Succinct Multi-Set Retrieval},
  author={Ramos, David Torres and Coleman, Benjamin and Lakshman, Vihan and Luo, Chen and Shrivastava, Anshumali},
  journal={arXiv preprint arXiv:2305.16545},
  year={2023}
}
```

Also, please let us know! We are strongly interested in making these data structures practical and efficient for real-world applications and would love to better accommodate your use case.
