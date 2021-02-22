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

#include <eidos/types.hpp>

#include "tcp.hpp"

namespace eidos {

/// response context
/// Redis protocol wrapper
class ResponseContext {
 private:
  std::shared_ptr<net::tcp::Socket> socket_;

 public:
  /// constructor
  /// \param socket socket connected with client
  explicit ResponseContext(std::shared_ptr<net::tcp::Socket> socket) : socket_(std::move(socket)) {}

 public:
  /// send ok response to client
  /// \tparam F response wrote callback function type
  /// \param on_write response wrote callback function
  template <class F>
  void ok(F&& on_write) {
    BOOST_LOG_TRIVIAL(trace) << "return simple string +OK";
    socket_->write("+OK\r\n", std::forward<F>(on_write));
  }

  /// send ok response with bytes value to client
  /// \tparam F response wrote callback function type
  /// \param value response bytes
  /// \param on_write response wrote callback function
  template <class F>
  void ok(const std::vector<std::byte>& value, F&& on_write) {
    BOOST_LOG_TRIVIAL(trace) << "return string +OK";
    socket_->write("$" + std::to_string(value.size()) + "\r\n" + eidos::BytesToString(value) + "\r\n",
                   std::forward<F>(on_write));
  }

  /// send ok response with array of bytes to client
  /// \tparam F response wrote callback function type
  /// \param value response bytes
  /// \param on_write response wrote callback function
  template <class F>
  void ok(const std::vector<std::vector<std::byte>>& value, F&& on_write) {
    std::stringstream ss;
    ss << "*" << value.size() << "\r\n";
    for (const auto& v : value) {
      ss << "$" << v.size() << "\r\n";
      ss << eidos::BytesToString(v) << "\r\n";
    }
    BOOST_LOG_TRIVIAL(trace) << "return string ok with array of bytes";
    socket_->write(ss.str(), std::forward<F>(on_write));
  }

  /// send error response to client
  /// \tparam F response wrote callback function type
  /// \param message error message
  /// \param on_write response wrote callback function
  template <class F>
  void err(const std::string& message, F&& on_write) {
    BOOST_LOG_TRIVIAL(trace) << "return string -ERR " << message;
    socket_->write("-ERR " + message + "\r\n", std::forward<F>(on_write));
  }

 public:
  /// send raw response to client
  /// \tparam F response wrote callback function type
  /// \param str raw string
  /// \param on_write response wrote callback function
  template <class F>
  void okRaw(const std::string& str, F&& on_write) {
    BOOST_LOG_TRIVIAL(trace) << "return string ok with raw response string";
    socket_->write(str, std::forward<F>(on_write));
  }
};

/// request context
/// Redis protocol wrapper
class RequestContext {
 private:
  std::shared_ptr<net::tcp::Socket> socket_;
  std::size_t param_count_;
  std::size_t current_read_;
  std::vector<std::vector<std::byte>> param_bytes_;

 public:
  explicit RequestContext(std::shared_ptr<net::tcp::Socket> socket)
      : socket_(std::move(socket)), param_count_(0), current_read_(0), param_bytes_() {}

 private:
  /// read command arguments
  /// \tparam F request read callback function type
  /// \param callback request read callback
  template <class F>
  void readParams(F&& callback) {
    socket_->readString("\r\n", [this, callback = std::forward<F>(callback)](const std::string& payload) {
      if (payload[0] != '$') {
        BOOST_LOG_TRIVIAL(error) << "invalid request. payload is not started with '$'.";
        BOOST_LOG_TRIVIAL(trace) << "additional info for error: payload: " << payload;
        return;
      }
      const auto length = std::stoul(payload.substr(1));
      BOOST_LOG_TRIVIAL(trace) << "parameter (" << (current_read_ + 1) << "/" << param_count_ << ") length";
      socket_->readBytes(length + 2,  // length + '\r' '\n'
                         [this, callback = std::forward<F>(callback)](const std::vector<std::byte>& bytes) {
                           BOOST_LOG_TRIVIAL(trace)
                               << "read parameter body " << (current_read_ + 1) << "/" << param_count_;
                           current_read_++;
                           param_bytes_.emplace_back(std::begin(bytes), std::end(bytes) - 2);
                           if (current_read_ >= param_count_) {
                             BOOST_LOG_TRIVIAL(trace) << "all parameters read. calling callback";
                             callback(param_bytes_);
                             return;
                           }
                           readParams(std::forward<F>(callback));
                         });
    });
  }

 public:
  /// read the request that contains command name and arguments
  /// \tparam F request read callback function type
  /// \param callback request read callback
  template <class F>
  void read(F&& callback) {
    socket_->readString("\r\n", [this, callback = std::forward<F>(callback)](const std::string& payload) {
      if (payload[0] != '*') {
        BOOST_LOG_TRIVIAL(error) << "invalid request. payload is not started with '*'.";
        BOOST_LOG_TRIVIAL(trace) << "additional info for error: payload: " << payload;
        return;
      }
      param_bytes_.clear();
      param_count_ = std::stoul(payload.substr(1));
      current_read_ = 0;
      BOOST_LOG_TRIVIAL(trace) << "param count";
      readParams(callback);
    });
  }

 public:
  /// create [ResponseContext] instance
  /// \return response context
  [[nodiscard]] std::shared_ptr<ResponseContext> response() { return std::make_shared<ResponseContext>(socket_); }
};

}  // namespace eidos