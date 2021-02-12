//
// Created by cerussite on 2/9/21.
//

#pragma once

#include <algorithm>
#include <filesystem>
#include <random>
#include <sstream>

#include "storage_base.hpp"

namespace eidos::storage {

namespace detail {

inline std::string GenerateUuid4() {
  static std::mt19937 mt(std::random_device{}());
  static std::uniform_int_distribution<std::uint16_t> rand16;
  const auto u16_1 = rand16(mt);
  const auto u16_2 = rand16(mt);
  const auto u16_3 = rand16(mt);
  const auto u16_4 = (rand16(mt) & 0x0111) | 0x4000;
  const auto u16_5 = (rand16(mt) & 0b00111111'11111111) | 0b10000000'00000000;
  const auto u16_6 = rand16(mt);
  const auto u16_7 = rand16(mt);
  const auto u16_8 = rand16(mt);

  char buf[37] = {};
  std::sprintf(buf, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x", u16_1, u16_2, u16_3,
               u16_4, u16_5, u16_6, u16_7, u16_8);
  return std::string(buf);
}

}  // namespace detail

class DirectoryStorageEngine : public StorageEngineBase {
 private:
  std::filesystem::path base_dir_;

 private:
  static std::filesystem::path GetDirectoryFromDigest(
      const std::vector<std::byte> &digest) {
    const auto to_hex = [](std::byte b) {
      std::stringstream ss;
      ss << std::hex << b;
      return ss.str();
    };
    std::string hex(digest.size() + 1, '\0');

    const auto digest_first_2 = std::begin(digest) + 3;
    auto hi = std::transform(std::begin(digest), digest_first_2,
                             std::begin(hex), to_hex);
    *hi = '/';
    hi++;
    std::transform(digest_first_2, std::end(digest), hi, to_hex);
    return base_dir_ / std::filesystem::path(hex);
  }

  static std::vector<std::byte> Read(std::istream &is) {
    std::uint32_t length = 0;
    is.read(&length, sizeof(length));
    std::vector<std::byte> bytes(length, std::byte{});
    is.read(bytes.data(), length);
    return bytes;
  }

  static std::tuple<Key, Value> ReadWhole(std::istream &is) {
    const auto key = Read(is);
    const auto value = Read(is);
    return std::make_tuple(Key(key, {}), Value(value));
  }

  static Result<std::filesystem::path> GetFromKey(const Key &key) {
    namespace fs = std::filesystem;

    const auto dir = GetDirectoryFromDigest(key.digest());
    if (!fs::exists(dir)) {
      return Result<fs::path>::Err("key not found (hash)");
    }
    for (const auto &entry : fs::directory_iterator(dir)) {
      std::ifstream ifs(entry.path());
      const auto file_key = Read(ifs);
      if (key.bytes() == file_key) {
        return Result<fs::path>::Ok(entry.path());
      }
    }
    return Result<fs::path>::Err("key not found (file)");
  }

 public:
  Result<Value> get(const Key &key) override {
    namespace fs = std::filesystem;
    const auto path_opt = GetFromKey(key);
    if (path_opt.is_err()) {
      return Result<Value>::Err("key not found");
    }
    std::ifstream ifs(path_opt.unwrap());
    const auto [_, value] = ReadWhole(ifs);
    return Result<Value>::Ok(value);
  }

  Result<void> set(const Key &key, const Value &value) override {
    namespace fs = std::filesystem;
    const auto path =
        GetFromKey(key).unwrap_or_else([&key](const std::string &) {
          return GetDirectoryFromDigest(key.digest()) / detail::GenerateUuid4();
        });
    std::ofstream ofs(path);
    key.writeTo(ofs);
    value.writeTo(ofs);
    return Result<void>::Ok();
  }

  Result<bool> exists(const Key &key) override {
    namespace fs = std::filesystem;
    const auto path_opt = GetFromKey(key);
    return Result<bool>::Ok(path_opt.is_ok());
  }

  Result<void> del(const Key &key) override {
    namespace fs = std::filesystem;

    const auto path = GetFromKey(key);
    if (path.is_err()) {
      return Result<void>::Err(path.err().value());
    }
    fs::remove(path.ok().value());
  }

  Result<std::vector<Key>> keys(const std::string &pattern) override {}
};

}  // namespace eidos::storage
