//
// Created by cerussite on 2/9/21.
//

#pragma once

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace eidos::result {

struct PanicError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace detail {

template <class T>
auto ToString(const T& v) -> decltype(v.to_string()) {
  return v.to_string();
}

template <class T>
auto ToString(const T& v) -> decltype(v.string()) {
  return v.string();
}

template <class T>
auto ToString(const T& v) -> decltype(v.str()) {
  return v.str();
}

template <class T, class = decltype(std::cout << std::declval<T>())>
std::string ToString(const T& v) {
  std::stringstream ss;
  ss << v;
  return ss.str();
}

inline std::string ToString(const std::runtime_error& e) { return e.what(); }

inline std::string ToString(const std::string& s) { return s; }

}  // namespace detail

template <class T, class E>
class Result {
 private:
  std::optional<T> data_;
  std::optional<E> error_;

 private:
  Result(std::optional<T> d, std::optional<E> e)
      : data_(std::move(d)), error_(std::move(e)) {}

 public:
  static Result Err(E error) { return Result(std::nullopt, std::move(error)); }
  static Result Ok(T data) { return Result(std::move(data), std::nullopt); }

 public:
  [[nodiscard]] bool is_ok() const noexcept { return !is_err(); }
  [[nodiscard]] bool is_err() const noexcept {
    return error_.has_value() && (!data_.has_value());
  }

 public:
  [[nodiscard]] const std::optional<T>& ok() const { return data_; }
  [[nodiscard]] const std::optional<E>& err() const { return error_; }

 public:
  T unwrap_or(T def) const {
    if (is_ok()) {
      return ok().value();
    }
    return def;
  }

  template <class F>
  T unwrap_or_else(F&& op) const {
    if (is_ok()) {
      return ok().value();
    }
    return op(err().value());
  }

  T expect(const std::string& msg) const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(msg + ": " + ToString(err().value()));
    }
    return ok().value();
  }

  T unwrap() const {
    using detail::ToString;

    if (is_err()) {
      throw PanicError(ToString(err().value()));
    }
    return ok().value();
  }
};

template <class E>
class Result<void, E> {
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
