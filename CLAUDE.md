# CaramelDB Build and Test Guide

## Building

### Python Bindings (Default)
```bash
bin/build.py
```
Builds the code in release mode and installs Python bindings.

### C++ Tests
```bash
bin/build.py -t all
```
Builds all targets including C++ tests. Required before running C++ tests.

### Build Modes
- `Release` (default)
- `Debug`
- `RelWithDebInfo`
- `DebugWithAsan`

Example: `bin/build.py -m Debug -t all`

## Testing

### Python Tests
```bash
bin/python-test.sh                           # Run all Python tests
bin/python-test.sh -k test_get_bloom_filter  # Run specific test
```

### C++ Tests
```bash
bin/cpp-test.sh                                                # Run all C++ tests
bin/cpp-test.sh -R BloomFilterTest.SimpleAddAndCheck          # Run specific test
bin/cpp-test.sh --output-on-failure                           # Show output on failure
```

## Build

After making changes to C++ code, you must rebuild before running tests:
```bash
bin/build.py
```

## Code Formatting

### Python
```bash
bin/python-format.sh
```
Formats Python code using black and isort.

# CaramelDB Code Style Guide

## Code Comments

Do NOT add superfluous comments to lines like 

// Update _num_elements to reflect actual number of keys added
_num_elements = _keys.size();

This is useless and doesn't help the reader. Only add comments when:
* we need to explain WHY something is happening or
* its unclear what is happening already from the code.

