#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <eidos/version.hpp>
#include <iostream>

#include "server.hpp"
#include "storage/memstore.hpp"

namespace {

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
     << "  directory : use directory style engine. (not implemented)\n"     //
     << "\n"                                                                //
     << "published under Apache License 2.0" << std::endl;
}

}  // namespace

int main(const int argc, const char* const* const argv) {
  using boost::program_options::value;
  boost::program_options::options_description options("eidos");
  options.add_options()              // options
      ("help,h", "show help")        // --help, -h: help
      ("version,v", "show version")  // --version, -v: version
      ("port,p", value<int>()->default_value(6379), "port number")  // port
      ("engine", value<std::string>()->default_value("memory"),
       "storage engine (memory)")  //
      ;
  boost::program_options::variables_map vm;
  boost::program_options::store(
      boost::program_options::parse_command_line(argc, argv, options), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    StreamHelp(std::cout, argv[0]);
    return EXIT_SUCCESS;
  }
  if (vm.count("version")) {
    std::cout << eidos::version::VersionInfo() << std::endl;
    return EXIT_SUCCESS;
  }

  BOOST_LOG_TRIVIAL(info) << "starting eidos server";

  boost::asio::io_context ioc;
  std::shared_ptr<eidos::storage::StorageEngineBase> engine;
  if (vm["engine"].as<std::string>() == "memory") {
    BOOST_LOG_TRIVIAL(info) << "storage engine: memory";
    engine = std::make_shared<eidos::storage::MemoryStorageEngine<>>();
  } else {
    BOOST_LOG_TRIVIAL(fatal) << "unknown engine name";
    return EXIT_FAILURE;
  }
  eidos::Serve(ioc, vm["port"].as<int>(), engine);
  ioc.run();
  return 0;
}
