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

#include "storage/storage_base.hpp"

namespace eidos {

/// listen and serve
/// \param ioc reference to instance of io_context
/// \param port listening port
/// \param engine storage engine
void Serve(boost::asio::io_context& ioc, std::uint16_t port, std::shared_ptr<eidos::storage::StorageEngineBase> engine);

}  // namespace eidos
