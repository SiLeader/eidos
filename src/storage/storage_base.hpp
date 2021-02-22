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

  /// dump all key value pairs
  /// \return key value pairs or error
  virtual Result<std::vector<std::tuple<Key, Value>>> dump() = 0;
};

}  // namespace eidos::storage
