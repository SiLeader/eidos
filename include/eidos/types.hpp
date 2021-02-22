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

#include <cstddef>
#include <iostream>
#include <vector>

namespace eidos {

class BytesMessage {
 private:
  std::vector<std::byte> bytes_;

 public:
  explicit BytesMessage(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

  BytesMessage(const BytesMessage&) = default;
  BytesMessage(BytesMessage&&) = default;

  BytesMessage& operator=(const BytesMessage&) = default;
  BytesMessage& operator=(BytesMessage&&) = default;

  ~BytesMessage() = default;

 public:
  [[nodiscard]] const std::vector<std::byte>& bytes() const { return bytes_; }

 public:
  void writeTo(std::ostream& os) const {
    const auto size = static_cast<std::uint32_t>(bytes_.size());
    os.write(static_cast<const char*>(static_cast<const void*>(&size)), sizeof(std::uint32_t));
    os.write(static_cast<const char*>(static_cast<const void*>(bytes_.data())), size);
  }
};

class Key : public BytesMessage {
 private:
  std::uint_fast64_t digest_;

 public:
  Key(std::vector<std::byte> data, std::uint_fast64_t digest) : BytesMessage(std::move(data)), digest_(digest) {}

 public:
  [[nodiscard]] std::uint_fast64_t digest() const { return digest_; }
};

class Value : public BytesMessage {
 public:
  using BytesMessage::BytesMessage;
};

template <class RandomAccessIterator>
std::string BytesToString(const RandomAccessIterator& first, const RandomAccessIterator& last) {
  std::string s(static_cast<std::size_t>(std::distance(first, last)), '\0');
  std::transform(first, last, std::begin(s), [](std::byte b) { return static_cast<char>(b); });
  return s;
}

inline std::string BytesToString(const std::vector<std::byte>& bytes) {
  return BytesToString(std::begin(bytes), std::end(bytes));
}

}  // namespace eidos
