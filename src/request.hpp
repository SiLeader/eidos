//
// Created by MiyaMoto on 2021/02/13.
//

#ifndef EIDOS_REQUEST_HPP
#define EIDOS_REQUEST_HPP

#include <cstddef>
#include <cstdint>
#include <eidos/types.hpp>
#include <memory>
#include <string>
#include <vector>

#include "server.hpp"

namespace eidos {

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

  const auto calculate_digest = [](const std::vector<std::byte>& k) -> std::uint_fast64_t {
    return std::hash<std::string>{}(eidos::BytesToString(k));
  };

  BOOST_LOG_TRIVIAL(trace) << "command '" << cmd << "' received";
  if (cmd == "GET") {
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

#endif  // EIDOS_REQUEST_HPP
