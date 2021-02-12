//
// Created by cerussite on 2/9/21.
//

#include <boost/asio.hpp>
#include <cstdint>
#include <iostream>
#include <string>

#include "storage/storage_base.hpp"

namespace {

template <class RandomAccessIterator>
std::string BytesToString(const RandomAccessIterator& first,
                          const RandomAccessIterator& last) {
  std::string s(last - first, '\0');
  std::transform(first, last, std::begin(s),
                 [](std::byte b) { return static_cast<char>(b); });
  return s;
}

std::string BytesToString(const std::vector<std::byte>& bytes) {
  return BytesToString(std::begin(bytes), std::end(bytes));
}

class Socket {
 private:
  boost::asio::ip::tcp::socket socket_;
  boost::asio::streambuf buffer_;

 public:
  explicit Socket(boost::asio::io_context& ioc) : socket_(ioc) {}

 public:
  [[nodiscard]] boost::asio::ip::tcp::socket& socket() { return socket_; }

 public:
  template <class F>
  void readBytes(const std::string& pattern, F&& on_read) {
    boost::asio::async_read_until(
        socket_, buffer_, pattern,
        [this, on_read = std::forward<F>(on_read)](
            const boost::system::error_code&, std::size_t length) {
          auto data =
              boost::asio::buffer_cast<const std::byte*>(buffer_.data());
          std::vector<std::byte> bytes(data, data + length);
          buffer_.consume(length);
          on_read(bytes);
        });
  }

  template <class F>
  void readBytes(std::size_t length, F&& on_read) {
    const auto callback = [this, on_read = std::forward<F>(on_read)](
                              const boost::system::error_code&,
                              std::size_t length) {
      auto data = boost::asio::buffer_cast<const std::byte*>(buffer_.data());
      std::vector<std::byte> bytes(data, data + length);
      buffer_.consume(length);
      on_read(bytes);
    };

    if (buffer_.size() >= length) {
      callback(boost::system::error_code{}, length);
      return;
    }
    boost::asio::async_read(socket_, buffer_,
                            boost::asio::transfer_exactly(length), callback);
  }

  template <class F>
  void readString(const std::string& pattern, F&& on_read) {
    boost::asio::async_read_until(
        socket_, buffer_, pattern,
        [this, on_read = std::forward<F>(on_read)](
            const boost::system::error_code&, std::size_t length) {
          auto data =
              boost::asio::buffer_cast<const std::byte*>(buffer_.data());
          std::string str = BytesToString(data, data + length);
          buffer_.consume(length);
          on_read(str);
        });
  }

 public:
  template <class F>
  void write(const std::string& s, F&& on_write) {
    boost::asio::async_write(socket_, boost::asio::buffer(s),
                             [on_write = std::forward<F>(on_write)](
                                 const boost::system::error_code& ec,
                                 std::size_t) { on_write(!ec.failed()); });
  }
};

class TcpServer {
 private:
  boost::asio::io_context& ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::function<void(std::shared_ptr<Socket>)> on_accept_;

 public:
  explicit TcpServer(boost::asio::io_context& ioc,
                     boost::asio::ip::tcp::endpoint ep)
      : ioc_(ioc), acceptor_(ioc, std::move(ep)) {}

 private:
  void accept() {
    auto sock = std::make_shared<Socket>(ioc_);
    acceptor_.async_accept(sock->socket(),
                           [this, sock](const boost::system::error_code&) {
                             on_accept_(sock);
                             ioc_.post([this] { accept(); });
                           });
  }

 public:
  template <class F>
  void listen(F&& on_accept) {
    on_accept_ = std::forward<F>(on_accept);
    accept();
  }
};

class ResponseContext {
 private:
  std::shared_ptr<Socket> socket_;

 public:
  explicit ResponseContext(std::shared_ptr<Socket> socket)
      : socket_(std::move(socket)) {}

 public:
  template <class F>
  void ok(F&& on_write) {
    socket_->write("+OK\r\n", std::forward<F>(on_write));
  }

  template <class F>
  void ok(const std::vector<std::byte>& value, F&& on_write) {
    socket_->write("$" + std::to_string(value.size()) + "\r\n" +
                       BytesToString(value) + "\r\n",
                   std::forward<F>(on_write));
  }

  template <class F>
  void ok(const std::vector<std::vector<std::byte>>& value, F&& on_write) {
    std::stringstream ss;
    ss << "*" << value.size() << "\r\n";
    for (const auto& v : value) {
      ss << "$" << v.size() << "\r\n";
      ss << BytesToString(v) << "\r\n";
    }
    socket_->write(ss.str(), std::forward<F>(on_write));
  }

  template <class F>
  void err(const std::string& message, F&& on_write) {
    socket_->write("-ERR " + message, std::forward<F>(on_write));
  }
};

class RequestContext {
 private:
  std::shared_ptr<Socket> socket_;
  std::size_t param_count_;
  std::size_t current_read_;
  std::vector<std::vector<std::byte>> param_bytes_;

 public:
  explicit RequestContext(std::shared_ptr<Socket> socket)
      : socket_(std::move(socket)) {}

 private:
  template <class F>
  void readParams(F&& callback) {
    socket_->readString("\r\n", [this, callback = std::forward<F>(callback)](
                                    const std::string& payload) {
      std::cout << "read p: " << payload << std::endl;
      if (payload[0] != '$') {
        return;
      }
      const auto length = std::stoul(payload.substr(1));
      socket_->readBytes(
          length + 2,  // length + '\r' '\n'
          [this, callback = std::forward<F>(callback)](
              const std::vector<std::byte>& bytes) {
            std::cout << "read pc: " << BytesToString(bytes) << std::endl;
            current_read_++;
            param_bytes_.emplace_back(std::begin(bytes), std::end(bytes) - 2);
            if (current_read_ >= param_count_) {
              callback(param_bytes_);
              return;
            }
            readParams(std::forward<F>(callback));
          });
    });
  }

 public:
  template <class F>
  void read(F&& callback) {
    socket_->readString("\r\n", [this, callback = std::forward<F>(callback)](
                                    const std::string& payload) {
      std::cout << "read: " << payload << std::endl;
      if (payload[0] != '*') {
        return;
        // TODO error
      }
      param_count_ = std::stoul(payload.substr(1));
      current_read_ = 0;
      readParams(callback);
    });
  }

 public:
  [[nodiscard]] std::shared_ptr<ResponseContext> response() {
    return std::make_shared<ResponseContext>(socket_);
  }
};

template <class F>
void OnRequest(std::shared_ptr<eidos::storage::StorageEngineBase> engine,
               std::shared_ptr<ResponseContext> res, const std::string& cmd,
               const std::vector<std::vector<std::byte>>& args, F&& callback) {
#define ARGS_LENGTH_ASSERT(len)                                       \
  do {                                                                \
    if (args.size() != len) {                                         \
      res->err("wrong number of arguments for '" + cmd + "' command", \
               std::forward<F>(callback));                            \
      return;                                                         \
    }                                                                 \
  } while (0)

  using eidos::Key;
  using eidos::Value;

  const auto calculate_digest =
      [](const std::vector<std::byte>& k) -> std::uint_fast64_t {
    return std::hash<std::string>{}(BytesToString(k));
  };

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
      res->ok(std::forward<F>(callback));
      return;
    }
    const auto err = result.err().value();
    res->err(err, std::forward<F>(callback));
    return;

  } else if (cmd == "DEL") {
    ARGS_LENGTH_ASSERT(1);

    Key key(args[0], calculate_digest(args[0]));
    auto result = engine->del(key);

  } else if (cmd == "KEYS") {
    ARGS_LENGTH_ASSERT(1);

    auto pattern = BytesToString(args[0]);
    auto result = engine->keys(pattern);

  } else {
    res->err("unknown command: " + cmd, std::forward<F>(callback));
    return;
  }
}  // namespace

void OnParamsRead(std::shared_ptr<eidos::storage::StorageEngineBase> engine,
                  std::shared_ptr<RequestContext> context,
                  const std::vector<std::vector<std::byte>>& params) {
  for (const auto& p : params) {
    std::cout << BytesToString(p) << std::endl;
  }
  auto res = context->response();
  auto cmd = BytesToString(params.front());
  std::transform(std::begin(cmd), std::end(cmd), std::begin(cmd), ::toupper);

  std::vector<std::vector<std::byte>> args(std::begin(params) + 1,
                                           std::end(params));
  OnRequest(std::move(engine), std::move(res), cmd, args,
            [context = std::move(context), engine](bool) {
              context->read(
                  [context,
                   engine](const std::vector<std::vector<std::byte>>& params) {
                    OnParamsRead(engine, context, params);
                  });
            });
}

}  // namespace

namespace eidos {

void Serve(boost::asio::io_context& ioc, std::uint16_t port,
           std::shared_ptr<eidos::storage::StorageEngineBase> engine) {
  auto server = std::make_shared<TcpServer>(
      ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
  server->listen([server, engine = std::move(engine)](
                     const std::shared_ptr<Socket>& socket) {
    std::cout << "on accept" << std::endl;
    auto context = std::make_shared<RequestContext>(socket);
    const auto callback =
        [context, engine](const std::vector<std::vector<std::byte>>& params) {
          OnParamsRead(engine, context, params);
        };

    context->read(callback);
  });
}

}  // namespace eidos
