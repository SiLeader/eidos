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

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <string>

#include "context.hpp"
#include "request.hpp"
#include "storage/storage_base.hpp"
#include "tcp.hpp"

namespace {

/// request parameter read event handler
/// \param engine storage engine
/// \param context request context
/// \param params parameters
void OnParamsRead(const std::shared_ptr<eidos::storage::StorageEngineBase>& engine,
                  std::shared_ptr<eidos::RequestContext> context, const std::vector<std::vector<std::byte>>& params) {
  auto res = context->response();
  auto cmd = eidos::BytesToString(params.front());
  // command name convert to uppercase
  std::transform(std::begin(cmd), std::end(cmd), std::begin(cmd), ::toupper);

  // repack command arguments
  std::vector<std::vector<std::byte>> args(std::begin(params) + 1, std::end(params));

  // call on request handler
  eidos::OnRequest(engine, res, cmd, args, [context = std::move(context), engine](bool) {
    // start to read next command
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
