//
// Created by cerussite on 2/10/21.
//

#pragma once

#include <boost/asio.hpp>

#include "storage/storage_base.hpp"

namespace eidos {

void Serve(boost::asio::io_context& ioc, std::uint16_t port,
           std::shared_ptr<eidos::storage::StorageEngineBase> engine);

}  // namespace eidos
