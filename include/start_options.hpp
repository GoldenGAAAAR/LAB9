// Copyright 2021 Your Name <your_email>

#ifndef INC_4_SEM_09_LAB_START_OPTIONS_HPP
#define INC_4_SEM_09_LAB_START_OPTIONS_HPP

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"
#include "parser.hpp"

namespace po = boost::program_options;
namespace fl = boost::filesystem;

class Options {
 private:
  po::options_description _description{"Allowed options"};
  po::variables_map _vm;

  std::string _url;
  std::string _file_path;
  size_t _producer_amount;
  size_t _consumer_amount;
  size_t _depth;

 public:
  Options(int argc, const char **argv);

  int exec();
};

#endif  // INC_4_SEM_09_LAB_START_OPTIONS_HPP
