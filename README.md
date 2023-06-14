# caramel.cpp

This repository provides a C++ implementation of the [CARAMEL data structure](https://arxiv.org/pdf/2305.16545.pdf), a space-efficient read-only retrieval table with applications to data management problems in recommender systems and computational biology. 

## Development Scripts
* `bin/build.py` is the main installation script which builds and installs the python package by default. 
* Running `bin/build.py` with `-t` specifies the target to build, for example `bin/build.py -t all` will build everything, including the C++ tests. When a target is specified, `bin/build.py` does not install the python package.
* To run all the python tests run `bin/python-test.sh` after `bin/build.py`. To run a specific test run `bin/python-test.sh -k <name_of_the_test_function_or_file>`.
* To run all the C++ tests run `bin/cpp-test.sh` after `bin/build.py -t all`. To run a specific test run `bin/cpp-test.sh -R <NameOfTheTestFixture>`.
*  Run `$ bin/cpp-format.sh` from anywhere to format all C++ code.
*  Run `$ bin/python-format.sh` from anywhere to format all Python code.
*  Run `$ bin/generate_compile_commands.sh` from anywhere to generate the compile
commands database (you shouldn't often need to do this manually, but try it
if your intellisense is acting strangely).