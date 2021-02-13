//
// Created by cerussite on 2/9/21.
//

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <iostream>
#include <string>

#include "context.hpp"
#include "request.hpp"
#include "storage/storage_base.hpp"
#include "tcp.hpp"

namespace {

void OnParamsRead(const std::shared_ptr<eidos::storage::StorageEngineBase>& engine,
                  std::shared_ptr<eidos::RequestContext> context, const std::vector<std::vector<std::byte>>& params) {
  auto res = context->response();
  auto cmd = eidos::BytesToString(params.front());
  std::transform(std::begin(cmd), std::end(cmd), std::begin(cmd), ::toupper);

  std::vector<std::vector<std::byte>> args(std::begin(params) + 1, std::end(params));
  eidos::OnRequest(engine, res, cmd, args, [context = std::move(context), engine](bool) {
    context->read([context, engine](const std::vector<std::vector<std::byte>>& params) {
      OnParamsRead(engine, context, params);
    });
  });
}

}  // namespace

namespace eidos {

void Serve(boost::asio::io_context& ioc, std::uint16_t port,
           std::shared_ptr<eidos::storage::StorageEngineBase> engine) {
  auto server =
      std::make_shared<net::tcp::Server>(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));

  server->listen([server, engine = std::move(engine)](const std::shared_ptr<net::tcp::Socket>& socket) {
    auto context = std::make_shared<RequestContext>(socket);
    const auto callback = [context, engine](const std::vector<std::vector<std::byte>>& params) {
      OnParamsRead(engine, context, params);
    };

    context->read(callback);
  });
  BOOST_LOG_TRIVIAL(info) << "listening on 0.0.0.0:" << port;
}

}  // namespace eidos
