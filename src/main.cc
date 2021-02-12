#include <iostream>

#include "server.hpp"
#include "storage/memstore.hpp"

int main() {
  boost::asio::io_context ioc;

  auto engine = std::make_shared<eidos::storage::MemoryStorageEngine<>>();
  eidos::Serve(ioc, 6378, engine);
  ioc.run();
  return 0;
}
