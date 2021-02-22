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

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace eidos::net::tcp {

/// TCP socket wrapper
class Socket {
 private:
  boost::asio::ip::tcp::socket socket_;
  boost::asio::streambuf buffer_;

 public:
  explicit Socket(boost::asio::io_context& ioc) : socket_(ioc) {}

 public:
  /// socket getter
  /// \return reference of socket
  [[nodiscard]] boost::asio::ip::tcp::socket& socket() { return socket_; }

 public:
  /// read bytes until pattern appeared
  /// \tparam F read event callback type
  /// \param pattern pattern
  /// \param on_read read event handler
  template <class F>
  void readBytes(const std::string& pattern, F&& on_read) {
    const auto endpoint = socket_.remote_endpoint();

    boost::asio::async_read_until(
        socket_, buffer_, pattern,
        [this, on_read = std::forward<F>(on_read), endpoint](const boost::system::error_code& ec, std::size_t length) {
          if (ec) {
            if (ec.value() == boost::asio::error::eof) {
              BOOST_LOG_TRIVIAL(trace) << "connection closed by peer (peer: " << endpoint << ")";
            } else {
              BOOST_LOG_TRIVIAL(error) << "read bytes error: " << ec << " (peer: " << endpoint << ")";
            }
            return;
          }
          BOOST_LOG_TRIVIAL(trace) << "read successful (" << length << " bytes) (peer: " << endpoint << ")";
          auto data = boost::asio::buffer_cast<const std::byte*>(buffer_.data());
          std::vector<std::byte> bytes(data, data + length);
          buffer_.consume(length);
          on_read(bytes);
        });
  }

  /// read [length] bytes
  /// \tparam F read event callback type
  /// \param length bytes length
  /// \param on_read read event handler
  template <class F>
  void readBytes(std::size_t length, F&& on_read) {
    const auto endpoint = socket_.remote_endpoint();

    const auto callback = [this, on_read = std::forward<F>(on_read), endpoint](const boost::system::error_code& ec,
                                                                               std::size_t length) {
      if (ec) {
        if (ec.value() == boost::asio::error::eof) {
          BOOST_LOG_TRIVIAL(trace) << "connection closed by peer (peer: " << endpoint << ")";
        } else {
          BOOST_LOG_TRIVIAL(error) << "read bytes error: " << ec << " (peer: " << endpoint << ")";
        }
        return;
      }
      BOOST_LOG_TRIVIAL(trace) << "read successful (" << length << " bytes) (peer: " << endpoint << ")";
      auto data = boost::asio::buffer_cast<const std::byte*>(buffer_.data());
      std::vector<std::byte> bytes(data, data + length);
      buffer_.consume(length);
      on_read(bytes);
    };

    if (buffer_.size() >= length) {
      // already read
      callback(boost::system::error_code{}, length);
      return;
    }
    boost::asio::async_read(socket_, buffer_, boost::asio::transfer_exactly(length), callback);
  }

  /// read string until patter appeared
  /// \tparam F read event callback type
  /// \param pattern pattern
  /// \param on_read read event handler
  template <class F>
  void readString(const std::string& pattern, F&& on_read) {
    const auto endpoint = socket_.remote_endpoint();

    boost::asio::async_read_until(
        socket_, buffer_, pattern,
        [this, on_read = std::forward<F>(on_read), endpoint](const boost::system::error_code& ec, std::size_t length) {
          if (ec) {
            if (ec.value() == boost::asio::error::eof) {
              BOOST_LOG_TRIVIAL(trace) << "connection closed by peer (peer: " << endpoint << ")";
            } else {
              BOOST_LOG_TRIVIAL(error) << "read string error: " << ec << " (peer: " << endpoint << ")";
            }
            return;
          }
          BOOST_LOG_TRIVIAL(trace) << "read successful (" << length << " bytes) (peer: " << endpoint << ")";
          auto data = boost::asio::buffer_cast<const std::byte*>(buffer_.data());
          std::string str = eidos::BytesToString(data, data + length);
          buffer_.consume(length);
          on_read(str);
        });
  }

 public:
  /// write to socket
  /// \tparam F wrote event handler type
  /// \param s value to write
  /// \param on_write wrote event handler
  template <class F>
  void write(const std::string& s, F&& on_write) {
    const auto endpoint = socket_.remote_endpoint();

    boost::asio::async_write(
        socket_, boost::asio::buffer(s),
        [on_write = std::forward<F>(on_write), endpoint](const boost::system::error_code& ec, std::size_t length) {
          if (ec) {
            BOOST_LOG_TRIVIAL(error) << "write error: " << ec << " (peer: " << endpoint << ")";
          } else {
            BOOST_LOG_TRIVIAL(trace) << length << " bytes wrote (peer: " << endpoint << ")";
          }
          on_write(!ec.failed());
        });
  }
};

/// TCP server (acceptor)
class Server {
 private:
  boost::asio::io_context& ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::function<void(std::shared_ptr<Socket>)> on_accept_;

 public:
  explicit Server(boost::asio::io_context& ioc, const boost::asio::ip::tcp::endpoint& ep)
      : ioc_(ioc), acceptor_(ioc, ep) {}

 private:
  /// start accept socket
  void accept() {
    auto sock = std::make_shared<Socket>(ioc_);
    acceptor_.async_accept(sock->socket(), [this, sock](const boost::system::error_code&) {
      const auto endpoint = sock->socket().remote_endpoint();
      BOOST_LOG_TRIVIAL(trace) << "client connected: " << endpoint;
      on_accept_(sock);
      ioc_.post([this] { accept(); });
    });
  }

 public:
  /// start listen
  /// \tparam F on accept event handler type
  /// \param on_accept on accept event handler
  template <class F>
  void listen(F&& on_accept) {
    on_accept_ = std::forward<F>(on_accept);
    accept();
  }
};

}  // namespace eidos::net::tcp
