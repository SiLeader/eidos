//
// Created by cerussite on 2/9/21.
//

#pragma once

#include <algorithm>
#include <list>
#include <unordered_map>

#include "storage_base.hpp"

namespace eidos::storage {

template <class Allocator = std::allocator<std::list<std::tuple<Key, Value>>>>
class MemoryStorageEngine : public StorageEngineBase {
 public:
  using allocator_type = Allocator;

 private:
  using Container = std::list<std::tuple<Key, Value>>;

 private:
  std::size_t bucket_size_;
  Container* storage_;
  Allocator allocator_;

 public:
  MemoryStorageEngine() : bucket_size_(1024), storage_(nullptr), allocator_() {
    expandAndRehash();
  }

 private:
  void expandAndRehash() {
    const auto size = bucket_size_ * 2;
    Container* const storage = allocator_.allocate(size);
    if (storage_ == nullptr) {
      for (std::size_t i = 0; i < size; ++i) {
        new (storage + i) Container();
      }
    } else {
      std::for_each(storage_, storage_ + bucket_size_,
                    [&storage, size](const Container& c) {
                      for (const auto& kv : c) {
                        const auto digest = std::get<0>(kv).digest();
                        storage[digest % size].emplace_back(kv);
                      }
                    });
    }
    storage_ = storage;
    bucket_size_ = size;
  }

 public:
  Result<Value> get(const Key& key) override {
    const auto& l = storage_[key.digest() % bucket_size_];
    for (const auto& [k, v] : l) {
      if (key.bytes() == k.bytes()) {
        return Result<Value>::Ok(v);
      }
    }
    return Result<Value>::Err("key not found");
  }

  Result<void> set(const Key& key, const Value& value) override {
    auto& l = storage_[key.digest() % bucket_size_];
    for (auto& [k, v] : l) {
      if (key.bytes() == k.bytes()) {
        v = value;
        return Result<void>::Ok();
      }
    }
    l.emplace_back(key, value);
    return Result<void>::Ok();
  }

  Result<bool> exists(const Key& key) override {
    auto& l = storage_[key.digest() % bucket_size_];
    for (auto itr = std::begin(l); itr != std::end(l); ++itr) {
      if (key.bytes() == std::get<0>(*itr).bytes()) {
        l.erase(itr);
        return Result<bool>::Ok(true);
      }
    }
    return Result<bool>::Ok(false);
  }

  Result<void> del(const Key& key) override {
    const auto& l = storage_[key.digest() % bucket_size_];
    for (const auto& [k, v] : l) {
      if (key.bytes() == k.bytes()) {
        return Result<void>::Ok();
      }
    }
    return Result<void>::Err("key not found");
  }

  Result<std::vector<Key>> keys(const std::string& pattern) override {
    return Result<std::vector<Key>>::Ok({});
  }
};

}  // namespace eidos::storage
