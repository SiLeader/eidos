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
#include <cstdint>
#include <eidos/types.hpp>
#include <memory>
#include <string>
#include <vector>

#include "server.hpp"

namespace eidos {

/// request received event callback function.
/// process Redis command.
/// \tparam F response wrote event callback function type
/// \param engine storage engine
/// \param res response context
/// \param cmd command name
/// \param args command arguments
/// \param callback response wrote event callback function
template <class F>
void OnRequest(const std::shared_ptr<eidos::storage::StorageEngineBase>& engine,
               const std::shared_ptr<ResponseContext>& res, const std::string& cmd,
               const std::vector<std::vector<std::byte>>& args, F&& callback) {
#define ARGS_LENGTH_ASSERT(len)                                                                    \
  do {                                                                                             \
    if (args.size() != len) {                                                                      \
      BOOST_LOG_TRIVIAL(error) << "invalid number of arguments for " << cmd << " (expect: " << len \
                               << ", actual: " << args.size() << ")";                              \
      res->err("wrong number of arguments for '" + cmd + "' command", std::forward<F>(callback));  \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

  using eidos::Key;
  using eidos::Value;

  // compute hash
  const auto calculate_digest = [](const std::vector<std::byte>& k) -> std::uint_fast64_t {
    return std::hash<std::string>{}(eidos::BytesToString(k));
  };

  BOOST_LOG_TRIVIAL(trace) << "command '" << cmd << "' received";
  if (cmd == "GET") {
    // GET key
    ARGS_LENGTH_ASSERT(1);

    Key key(args[0], calculate_digest(args[0]));
    auto result = engine->get(key);
    if (result.is_ok()) {
      auto v = result.unwrap();
      res->ok(v.bytes(), std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;

  } else if (cmd == "SET") {
    // SET key value
    ARGS_LENGTH_ASSERT(2);

    Key key(args[0], calculate_digest(args[0]));
    Value value(args[1]);
    auto result = engine->set(key, value);
    if (result.is_ok()) {
      res->ok(std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;

  } else if (cmd == "EXISTS") {
    // EXISTS key
    ARGS_LENGTH_ASSERT(1);

    Key key(args[0], calculate_digest(args[0]));
    auto result = engine->exists(key);
    if (result.is_ok()) {
      res->okRaw(result.unwrap() ? ":1\r\n" : ":0\r\n", std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;

  } else if (cmd == "DEL") {
    // DEL key
    ARGS_LENGTH_ASSERT(1);

    Key key(args[0], calculate_digest(args[0]));
    auto result = engine->del(key);
    if (result.is_ok()) {
      res->ok(std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;

  } else if (cmd == "KEYS") {
    // KEYS pattern
    ARGS_LENGTH_ASSERT(1);

    auto pattern = eidos::BytesToString(args[0]);
    auto result = engine->keys(pattern);
    if (result.is_ok()) {
      const auto keys = result.unwrap();
      std::vector<std::vector<std::byte>> keys_bytes(keys.size());
      std::transform(std::begin(keys), std::end(keys), std::begin(keys_bytes),
                     [](const Key& key) { return key.bytes(); });
      res->ok(keys_bytes, std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;
  } else if (cmd == "COMMAND") {
    // COMMAND
    // redis-cli send this command before any commands
    static constexpr char NL[] = "\r\n";
    std::stringstream ss;
    ss << "*5" << NL         //
                             //
       << "*7" << NL         // 1 get
       << "$3" << NL         //
       << "get" << NL        // get
       << ":1" << NL         // arity
       << "*1" << NL         //
       << "+readonly" << NL  //
       << ":1" << NL << ":1" << NL << ":0" << NL << "*0"
       << NL  //
       //
       << "*7" << NL        // 2 set
       << "$3" << NL        //
       << "set" << NL       // set
       << ":2" << NL        // arity
       << "*2" << NL        //
       << "+write" << NL    //
       << "+denyoom" << NL  //
       << ":1" << NL << ":1" << NL << ":0" << NL << "*0"
       << NL  //
       //
       << "*7" << NL         // 3 exists
       << "$6" << NL         //
       << "exists" << NL     // exists
       << ":1" << NL         // arity
       << "*1" << NL         //
       << "+readonly" << NL  //
       << ":1" << NL << ":1" << NL << ":0" << NL << "*0"
       << NL  //
       //
       << "*7" << NL      // 4 del
       << "$3" << NL      //
       << "del" << NL     // del
       << ":1" << NL      // arity
       << "*1" << NL      //
       << "+write" << NL  //
       << ":1" << NL << ":1" << NL << ":0" << NL << "*0"
       << NL  //
       //
       << "*7" << NL                                             // 5 keys
       << "$4" << NL                                             //
       << "keys" << NL                                           // keys
       << ":1" << NL                                             // arity
       << "*1" << NL                                             //
       << "+readonly" << NL                                      //
       << ":0" << NL << ":0" << NL << ":0" << NL << "*0" << NL;  //
    res->okRaw(ss.str(), std::forward<F>(callback));
  } else {
    BOOST_LOG_TRIVIAL(error) << "unknown command '" << cmd << "'";
    res->err("unknown command: " + cmd, std::forward<F>(callback));
    return;
  }
#undef ARGS_LENGTH_ASSERT
}

}  // namespace eidos
