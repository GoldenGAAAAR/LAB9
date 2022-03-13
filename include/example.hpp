// Copyright 2021 Your Name <your_email>

#ifndef INCLUDE_EXAMPLE_HPP_
#define INCLUDE_EXAMPLE_HPP_
#include <set>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl.hpp>
#include <utility>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include "gumbo.h"
#include "testt.h"
#include <iomanip>

namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

class Parser {
 private:
  std::string _url;
  std::string _host;
  std::string _port;
  std::string _target;
  std::string _type;
  std::set<std::string> _urls;
  std::set<std::string> _picture_urls;

 public:
  explicit Parser(std::string url) : _url(std::move(url)) {}

  void prepare () {
    auto protocol_len = 7;
    auto url = _url;

    if (_url.substr(0, 5) == "https") {
      _type = "https";
      _port = "443";
      ++protocol_len;
    } else {
      _type = "http";
      _port = "80";
    }

    url.erase(0, protocol_len);


  }


};


std::string find_host(std::string url) {
  std::string type = url.substr(0, 5);
  if (type == "https") {
    url.erase(0, 8);
  } else {
    url.erase(0, 7);
  }
  size_t n = url.find('/');
  if (n != std::string::npos) {
      return url.substr(0, n);
  } else {
    return url;
  }
}

std::string find_target(std::string url) {
  std::string type = url.substr(0, 5);
  auto protocol_len = 7;
  auto len = url.length();
  if (type == "https") {
    ++protocol_len;
  }
  url.erase(0, protocol_len);
  size_t n = url.find('/');
  if (n != std::string::npos) {
    return url.substr(n, len - protocol_len - n);
  } else {
    return "/";
  }
}

void gumbo_find_url(GumboNode* node, std::string& parent_url,
              std::set<std::string>& urls) {
  if (node->type == GUMBO_NODE_ELEMENT) {
    GumboAttribute* href;
    std::string current_url = parent_url;
    if (node->v.element.tag == GUMBO_TAG_A &&
        (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
      auto str_href = std::string(href->value);
      if (str_href.length() > 4 && str_href.substr(0, 4) == "http") {
        urls.insert(str_href);
      } else {
        current_url += str_href;
        urls.insert(current_url);
      }
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
      gumbo_find_url(static_cast<GumboNode*>(children->data[i]), current_url, urls);
    }
  }
}

void find_url(std::string html, std::string& url) {
  GumboOutput* outPut = gumbo_parse(html.c_str());
  std::set<std::string> urls;
  gumbo_find_url(outPut->root, url, urls);

  size_t i = 1;
  for (const auto& current_url : urls) {
    std::cout <<std::setw(2) << i++ << ") " << current_url << std::endl;
  }
}

void gumbo_find_pictures_urls(GumboNode* node, std::string& parent_url,
                        std::set<std::string>& urls) {
  if (node->type == GUMBO_NODE_ELEMENT) {
    GumboAttribute* src;
    if (node->v.element.tag == GUMBO_TAG_IMG &&
        (src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
      auto str_src = std::string(src->value);
      if (str_src.length() > 4 && str_src.substr(0, 4) == "http") {
        urls.insert(str_src);
      } else {
        urls.insert(parent_url + str_src);
      }
    }
    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
      gumbo_find_pictures_urls(static_cast<GumboNode*>(children->data[i]), parent_url,
                         urls);
    }
  }
}

void find_pictures_urls(std::string html, std::string& url) {
  GumboOutput* outPut = gumbo_parse(html.c_str());
  std::set<std::string> urls;
  std::string parent_url = find_type(url) + "://" + find_host(url);
  gumbo_find_pictures_urls(outPut->root, parent_url, urls);

  size_t i = 1;
  for (const auto& current_url : urls) {
    std::cout <<std::setw(2) << i++ << ") " << current_url << std::endl;
  }
}

std::string download_http(const std::string& host,
                   const std::string& target) {
  const std::string port = "80";
  boost::asio::io_context context;
  ip::tcp::resolver resolver{context};
  ip::tcp::socket socket{context};

  auto const result = resolver.resolve(host, port);

  boost::asio::connect(socket, result.begin(), result.end());

  http::request<http::string_body> req{http::verb::get, target, 10};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  http::write(socket, req);

  boost::beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(socket, buffer, res);

  // Gracefully close the socket
  boost::system::error_code ec;
  socket.shutdown(ip::tcp::socket::shutdown_both, ec);
  return boost::beast::buffers_to_string(res.body().data());
}

std::string download_https(const std::string& host,
                    const std::string& target) {
  int version = 10;
  const std::string port = "443";

  // The io_context is required for all I/O
  boost::asio::io_context context;

  // The SSL context is required, and holds certificates
  ssl::context ssl_context{ssl::context::sslv23_client};

  // This holds the root certificate used for verification
  load_root_certificates(ssl_context);

  // These objects perform our I/O
  ip::tcp::resolver resolver{context};
  ssl::stream<ip::tcp::socket> stream{context, ssl_context};

  // Look up the domain name
  auto const results = resolver.resolve(host, port);

  // Make the connection on the IP address we get from a lookup
  boost::asio::connect(stream.next_layer(), results.begin(), results.end());

  // Perform the SSL handshake
  stream.handshake(ssl::stream_base::client);

  // Set up an HTTP GET request message
  http::request<http::string_body> req{http::verb::get, target, version};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // Send the HTTP request to the remote host
  http::write(stream, req);

  // This buffer is used for reading and must be persisted
  boost::beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::dynamic_body> res;

  // Receive the HTTP response
  http::read(stream, buffer, res);

  // Gracefully close the stream
  boost::system::error_code ec;
  stream.shutdown(ec);

  return boost::beast::buffers_to_string(res.body().data());
}
// https://bmstu.ru/for-students/photonics
std::string download_page(std::string url) {
  std::string host = find_host(url);
  std::string target = find_target(url);
  std::string type = url.substr(0, 5);
  if (type == "https") {
    return download_https(host, target);
  } else if (type == "http:") {
    return download_http(host, target);
  } else {
    std::cout << "Start address error: " << url << std::endl;
    exit(1);
  }
}

void generator(std::string url) {
//  find_url(download_page(url), url);
  find_pictures_urls(download_page(url), url);
}

#endif  // INCLUDE_EXAMPLE_HPP_
