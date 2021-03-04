// Copyright 2021 SiLeader and Cerussite.
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an “AS IS” BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace eidos::result {

struct PanicError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace detail {
// string conversion helpers

/// convert to string (using `to_string` member function)
/// \tparam T string convertible type (has `to_string` member function)
/// \param v requested value
/// \return converted string
template <class T>
auto ToString(const T& v) -> decltype(v.to_string()) {
  return v.to_string();
}

/// convert to string (using `string` member function)
/// \tparam T string convertible type (has `string` member function)
/// \param v requested value
/// \return converted string
template <class T>
auto ToString(const T& v) -> decltype(v.string()) {
  return v.string();
}

/// convert to string (using `str` member function)
/// \tparam T string convertible type (has `str` member function)
/// \param v requested value
/// \return converted string
template <class T>
auto ToString(const T& v) -> decltype(v.str()) {
  return v.str();
}

/// convert to string (using ostream operator (`<<`))
/// \tparam T string convertible type (has `operator<<` overloading to std::ostream)
/// \param v requested value
/// \return converted string
template <class T, class = decltype(std::cout << std::declval<T>())>
std::string ToString(const T& v) {
  std::stringstream ss;
  ss << v;
  return ss.str();
}

/// convert to string from subtypes of std::runtime_error
/// \param e runtime_error instance
/// \return `e.what()`
inline std::string ToString(const std::runtime_error& e) { return e.what(); }

/// string to string helper
/// \param s string
/// \return same with e
inline std::string ToString(const std::string& s) { return s; }

}  // namespace detail

/// Rust std::result::Result like class
/// \tparam T success value
/// \tparam E error value
template <class T, class E>
class Result {
 private:
  std::variant<T, E> data_;

 private:
  // constructors

  // construct from success value
  explicit Result(const T& d) : data_(d) {}
  explicit Result(T&& d) : data_(std::move(d)) {}

  // construct from error value
  explicit Result(const E& d) : data_(d) {}
  explicit Result(E&& d) : data_(std::move(d)) {}

 public:
  /// create error instance
  /// \param error error value
  /// \return Result instance (error)
  static Result Err(E error) { return Result(std::move(error)); }

  /// create success instance
  /// \param data success value
  /// \return Result instance (ok)
  static Result Ok(T data) { return Result(std::move(data)); }

 public:
  /// check operation result is ok
  /// \return true: ok, false: error
  [[nodiscard]] bool is_ok() const noexcept { return data_.index() == 0; }

  /// check operation result is error
  /// \return true: error, false: ok
  [[nodiscard]] bool is_err() const noexcept { return !is_ok(); }

 public:
  /// get success value
  /// \return success value or no value
  [[nodiscard]] std::optional<T> ok() const {
    if (is_ok()) {
      return std::get<0>(data_);
    }
    return std::nullopt;
  }

  /// get error value
  /// \return error value or no value
  [[nodiscard]] std::optional<E> err() const {
    if (is_err()) {
      return std::get<1>(data_);
    }
    return std::nullopt;
  }

 public:
  /// get success value or default value
  /// \param def default value
  /// \return is_ok() == true: success value, is_ok() == false: default value
  T unwrap_or(const T& def) const {
    if (is_ok()) {
      return ok().value();
    }
    return def;
  }

  /// get success value or value that returned from [op]
  /// \tparam F op function type (`T op(E)`)
  /// \param op function
  /// \return is_ok() == true: success value, is_ok() == false: op(error value)
  template <class F>
  T unwrap_or_else(F&& op) const {
    if (is_ok()) {
      return ok().value();
    }
    return op(err().value());
  }

  /// get success value or throw [PanicError]
  /// \param msg exception message
  /// \return success value or throw [PanicError]
  T expect(const std::string& msg) const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(msg + ": " + ToString(err().value()));
    }
    return ok().value();
  }

  /// get success value or throw [PanicError]
  /// \return success value or throw [PanicError]
  T unwrap() const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(ToString(err().value()));
    }
    return ok().value();
  }
};

/// partial specialization for `success value type == void`
/// \tparam E error type
template <class E>
class Result<void, E> {
 private:
  std::optional<E> error_;

 private:
  explicit Result(std::optional<E> e) : error_(std::move(e)) {}

 public:
  static Result Err(E error) { return Result(std::move(error)); }
  static Result Ok() { return Result(std::nullopt); }

 public:
  [[nodiscard]] bool is_ok() const noexcept { return !is_err(); }
  [[nodiscard]] bool is_err() const noexcept { return error_.has_value(); }

 public:
  [[nodiscard]] const std::optional<E>& err() const { return error_; }

 public:
  void expect(const std::string& msg) const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(msg + ": " + ToString(err().value()));
    }
  }

  void unwrap() const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(ToString(err().value()));
    }
  }
};

}  // namespace eidos::result
