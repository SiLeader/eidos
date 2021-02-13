//
// Created by cerussite on 2/9/21.
//

#pragma once

#include <eidos/result.hpp>
#include <eidos/types.hpp>

namespace eidos::storage {

///
/// base class of storage engines
///
class StorageEngineBase {
 public:
  template <class T>
  using Result = eidos::result::Result<T, std::string>;

  virtual ~StorageEngineBase() = default;

 public:
  /// set value to storage
  /// \param key key
  /// \param value value
  /// \return Result of operation
  virtual Result<void> set(const Key& key, const Value& value) = 0;

  /// get value from storage
  /// \param key key
  /// \return Result of operation
  virtual Result<Value> get(const Key& key) = 0;

  /// delete value from storage
  /// \param key key
  /// \return Result of operation
  virtual Result<void> del(const Key& key) = 0;

  /// get key existence status
  /// \param key key
  /// \return Result of operation
  virtual Result<bool> exists(const Key& key) = 0;

  /// get keys that match specified pattern
  /// \param pattern key pattern
  /// \return Result of operation
  virtual Result<std::vector<Key>> keys(const std::string& pattern) = 0;
};

}  // namespace eidos::storage
