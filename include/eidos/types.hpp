//
// Created by cerussite on 2/9/21.
//

#pragma once

#include <cstddef>
#include <iostream>
#include <vector>

namespace eidos {

class BytesMessage {
 private:
  std::vector<std::byte> bytes_;

 public:
  explicit BytesMessage(std::vector<std::byte> bytes)
      : bytes_(std::move(bytes)) {}

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
    os.write(static_cast<const char*>(static_cast<const void*>(&size)),
             sizeof(std::uint32_t));
    os.write(static_cast<const char*>(static_cast<const void*>(bytes_.data())),
             size);
  }
};

class Key : public BytesMessage {
 private:
  std::uint_fast64_t digest_;

 public:
  Key(std::vector<std::byte> data, std::uint_fast64_t digest)
      : BytesMessage(std::move(data)), digest_(digest) {}

 public:
  [[nodiscard]] std::uint_fast64_t digest() const { return digest_; }
};

class Value : public BytesMessage {
 public:
  using BytesMessage::BytesMessage;
};

}  // namespace eidos