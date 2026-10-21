#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace caramel::build {

using Keys = std::vector<std::string>;

// A non-owning view of the values to encode, one row per key. Dense rows have
// num_cols values each, stored row-major or column-major. Ragged rows (CSR:
// row r is data[offsets[r] .. offsets[r+1])) hold a variable number of
// values; column c of a ragged row exists only when c < length(r).
template <typename T> class Rows {
public:
  static Rows single(const std::vector<T> &values) {
    return rowMajor(values.data(), values.size(), 1);
  }

  static Rows rowMajor(const T *data, uint64_t num_rows, uint32_t num_cols) {
    Rows rows(num_rows, num_cols);
    rows._stride = num_cols;
    for (uint32_t c = 0; c < num_cols; c++) {
      rows._col_base[c] = data + c;
    }
    return rows;
  }

  static Rows columns(const std::vector<std::vector<T>> &cols) {
    if (cols.empty()) {
      throw std::invalid_argument("Need at least one column.");
    }
    Rows rows(cols[0].size(), cols.size());
    rows._stride = 1;
    for (uint32_t c = 0; c < cols.size(); c++) {
      if (cols[c].size() != rows._num_rows) {
        throw std::invalid_argument("All columns must have the same length.");
      }
      rows._col_base[c] = cols[c].data();
    }
    return rows;
  }

  // offsets has num_rows + 1 entries.
  static Rows ragged(const T *data, const uint64_t *offsets, uint64_t num_rows) {
    uint64_t max_len = 0;
    for (uint64_t r = 0; r < num_rows; r++) {
      max_len = std::max(max_len, offsets[r + 1] - offsets[r]);
    }
    Rows rows(num_rows, static_cast<uint32_t>(max_len));
    rows._data = data;
    rows._offsets = offsets;
    return rows;
  }

  uint64_t numRows() const { return _num_rows; }
  uint32_t numCols() const { return _num_cols; }
  bool isRagged() const { return _offsets != nullptr; }

  uint32_t length(uint64_t r) const {
    return _offsets ? static_cast<uint32_t>(_offsets[r + 1] - _offsets[r])
                    : _num_cols;
  }

  bool present(uint64_t r, uint32_t c) const {
    return !_offsets || c < length(r);
  }

  const T &at(uint64_t r, uint32_t c) const {
    return _offsets ? _data[_offsets[r] + c] : _col_base[c][r * _stride];
  }

private:
  Rows(uint64_t num_rows, uint32_t num_cols)
      : _num_rows(num_rows), _num_cols(num_cols), _col_base(num_cols) {}

  uint64_t _num_rows;
  uint32_t _num_cols;
  std::vector<const T *> _col_base;
  uint64_t _stride = 0;
  const T *_data = nullptr;
  const uint64_t *_offsets = nullptr;
};

} // namespace caramel::build
