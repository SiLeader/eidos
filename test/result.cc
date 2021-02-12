//
// Created by cerussite on 2/9/21.
//

#include <gtest/gtest.h>

#include <eidos/result.hpp>

TEST(EidosResult, Ok_is_ok_true) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.is_ok(), true);
}

TEST(EidosResult, Ok_is_err_false) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.is_err(), false);
}

TEST(EidosResult, Err_is_ok_false) {
  const auto res = eidos::result::Result<int, std::string>::Err("error");
  EXPECT_EQ(res.is_ok(), false);
}

TEST(EidosResult, Err_is_err_true) {
  const auto res = eidos::result::Result<int, std::string>::Err("error");
  EXPECT_EQ(res.is_err(), true);
}

TEST(EidosResult, Ok_unwrap_or) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.unwrap_or(5), 0);
}

TEST(EidosResult, Err_unwrap_or) {
  const auto res = eidos::result::Result<int, std::string>::Err("error");
  EXPECT_EQ(res.unwrap_or(5), 5);
}

TEST(EidosResult, Ok_unwrap_or_else) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.unwrap_or_else([](const std::string& s) { return s.size(); }),
            0);
}

TEST(EidosResult, Err_unwrap_or_else) {
  using namespace std::string_literals;
  const auto err = "error"s;
  const auto res = eidos::result::Result<int, std::string>::Err(err);
  EXPECT_EQ(res.unwrap_or_else([](const std::string& s) { return s.size(); }),
            err.size());
}

TEST(EidosResult, Ok_expect) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.expect("eval failed"), 0);
}

TEST(EidosResult, Err_expect) {
  const auto res = eidos::result::Result<int, std::string>::Err("error");
  EXPECT_THROW(res.expect("eval failed"), eidos::result::PanicError);
}

TEST(EidosResult, Ok_unwrap) {
  const auto res = eidos::result::Result<int, std::string>::Ok(0);
  EXPECT_EQ(res.unwrap(), 0);
}

TEST(EidosResult, Err_unwrap) {
  const auto res = eidos::result::Result<int, std::string>::Err("error");
  EXPECT_THROW(res.unwrap(), eidos::result::PanicError);
}

TEST(EidosResult, Err_unwrap_int) {
  const auto res = eidos::result::Result<int, int>::Err(1);
  EXPECT_THROW(res.unwrap(), eidos::result::PanicError);
}
