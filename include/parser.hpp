// Copyright 2021 Your Name <your_email>

#ifndef INCLUDE_PARSER_HPP_
#define INCLUDE_PARSER_HPP_

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>
#include <set>
#include <string>
#include <utility>

#include "ThreadPool.hpp"
#include "boost/filesystem.hpp"
#include "certificate.hpp"
#include "gumbo.h"

#define RED "\033[31m"
#define STOP "\033[0m"
#define GREEN "\033[32m"

namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

struct Url_data {
  std::string url;
  size_t depth{};
};

struct Html_data {
  std::string html_code;
  size_t depth{};
};

extern std::queue<Url_data> _urls;
extern std::queue<Html_data> _html;
extern std::set<std::string> _picture_urls;
extern std::string _type;
extern std::string _url;
extern std::string _host;
extern std::string _port;
extern std::string _target;
extern std::mutex _mutex;

class Parser {
 private:
  std::string _filename;
  size_t _depth;

  ThreadPool _producer_pool;
  ThreadPool _consumer_pool;

 public:
  explicit Parser(std::string url, size_t network_amount, size_t parser_amount,
                  size_t depth, std::string filename);

 private:
  static void prepare();

 public:
  void Run();

 private:
  static void Producer(const Url_data& data);

  static void Consumer(const Html_data& data, size_t* in_process);

  void write_in_file();

  static std::string download_http(const std::string& target);

  static std::string download_https(const std::string& target);

  static void gumbo_find_url(GumboNode* node, std::string& parent_url,
                             std::set<std::string>& urls);

  static std::set<std::string> find_url(const std::string& html);

  static void gumbo_find_pictures_urls(GumboNode* node, std::string& parent_url,
                                       std::set<std::string>& urls);

  static std::set<std::string> find_pictures_urls(const std::string& html);

  static std::string download_page(const std::string& url);

  static std::string find_target(std::string url);
};

#endif  // INCLUDE_PARSER_HPP_
