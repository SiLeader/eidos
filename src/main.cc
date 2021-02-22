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

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <eidos/version.hpp>
#include <iostream>

#include "server.hpp"
#include "storage/memstore.hpp"
#include "storage/raft.hpp"

namespace {

/// output help message to stream
/// \param os output stream
/// \param program program name (argv[0])
void StreamHelp(std::ostream& os, const char* program) {
  os << "usage: " << program << " [-hv] [--engine ENGINE] [--port PORT]\n"  //
     << "\n"                                                                //
     << "options\n"                                                         //
     << "  --help, -h           : show this help message\n"                 //
     << "  --version, -v        : show version\n"                           //
     << "  --port PORT, -p PORT : set port number (default: 6379)\n"        //
     << "  --engine ENGINE      : set storage engine (default: memory)\n"   //
     << "\n"                                                                //
     << "storage engine\n"                                                  //
     << "  memory    : use program heap memory as data storage.\n"          //
     << "  raft      : use Raft replicated in-memory storage\n"             //
     << "\n"                                                                //
     << "published under Apache License 2.0" << std::endl;
}

}  // namespace

int main(const int argc, const char* const* const argv) {
  // parsing command line arguments
  using boost::program_options::value;
  boost::program_options::options_description options("eidos");
  options.add_options()                                                       // options
      ("help,h", "show help")                                                 // --help, -h: help
      ("version,v", "show version")                                           // --version, -v: version
      ("port,p", value<std::uint16_t>()->default_value(6379), "port number")  // port
      ("engine", value<std::string>()->default_value("memory"),
       "storage engine (memory)")  //
      ;
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    StreamHelp(std::cout, argv[0]);
    return EXIT_SUCCESS;
  }
  if (vm.count("version")) {
    eidos::version::VersionInfo(std::cout);
    return EXIT_SUCCESS;
  }

  // start server
  BOOST_LOG_TRIVIAL(info) << "starting eidos server";

  boost::asio::io_context ioc;
  std::shared_ptr<eidos::storage::StorageEngineBase> engine;
  if (vm["engine"].as<std::string>() == "memory") {
    BOOST_LOG_TRIVIAL(info) << "storage engine: memory";
    engine = std::make_shared<eidos::storage::MemoryStorageEngine<>>();
  } else if (vm["engine"].as<std::string>() == "raft") {
    BOOST_LOG_TRIVIAL(info) << "storage engine: raft";
    engine = std::make_shared<eidos::storage::RaftStorageEngine>(
        std::make_shared<eidos::storage::MemoryStorageEngine<>>(), 16379);
  } else {
    BOOST_LOG_TRIVIAL(fatal) << "unknown engine name";
    return EXIT_FAILURE;
  }

  // start listen and serve
  eidos::Serve(ioc, vm["port"].as<std::uint16_t>(), engine);
  ioc.run();
  return 0;
}
