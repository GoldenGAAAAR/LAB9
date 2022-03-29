// Copyright 2021 Your Name <your_email>

#include "start_options.hpp"

Options::Options(int argc, const char **argv) {
  _description.add_options()
      ("help,h", "print usage message")
      ("url,u", po::value<std::string>(&_url), "website url")
      ("network_amount,n", po::value<size_t>(&_producer_amount), "producer amount")
      ("parser_amount,p", po::value<size_t>(&_consumer_amount), "consumer amount")
      ("depth,d", po::value<size_t>(&_depth), "depth of parsing")
      ("file_path,f", po::value<std::string>(&_file_path), "path to file");
  po::store(po::parse_command_line(argc, argv, _description), _vm);
  po::notify(_vm);
}

int Options::exec() {
  if (_vm.count("network_amount") && _vm.count("parser_amount") &&
      _vm.count("depth") && _vm.count("file_path") && _vm.count("url")) {
    Parser parser(_url, _producer_amount, _consumer_amount, _depth, _file_path);
    parser.Run();

    return 0;
  } else if (_vm.count("help")) {
    std::cout << _description << std::endl;
    return 1;
  } else {
    std::cout << "please, use --help or -h option for information" << std::endl;
    return 1;
  }
}
