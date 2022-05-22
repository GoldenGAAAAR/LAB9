//Copyright 2021 Your Name <your_email>

#include <parser.hpp>

std::queue<Url_data> _urls;
std::queue<Html_data> _html;
std::set<std::string> _picture_urls;
std::string _type;
std::string _url;
std::string _host;
std::string _port;
std::string _target;
std::mutex _mutex;

Parser::Parser(std::string url, size_t network_amount, size_t parser_amount,
               size_t depth, std::string filename)
    : _filename(std::move(filename)),
      _depth(depth),
      _producer_pool(network_amount),
      _consumer_pool(parser_amount) {
  _url = std::move(url);
}

void Parser::prepare() {
  size_t protocol_len = 7;
  std::string url = _url;
  size_t len = url.length();

  if (_url.substr(0, 5) == "https") {
    _type = "https";
    _port = "443";
    ++protocol_len;
  } else {
    _type = "http";
    _port = "80";
  }

  url.erase(0, protocol_len);
  size_t n = url.find('/');

  if (n != std::string::npos) {
    _host = url.substr(0, n);
    _target = url.substr(n, len - protocol_len - n);
  } else {
    _host = url;
    _target = "/";
  }

  Url_data data;
  data.depth = 0;
  data.url = _url;
  _urls.push(data);
}

void Parser::Run() {
  prepare();
  size_t in_process = 0;
  auto start = std::chrono::steady_clock::now();
  while (true) {
    if (!_urls.empty()) {
      _mutex.lock();
      Url_data data = _urls.front();
      _urls.pop();
      _mutex.unlock();

      if (data.depth == _depth) {
        continue;
      }

      in_process++;
      _producer_pool.enqueue(Producer, data);
    }
    if (!_html.empty()) {
      _mutex.lock();
      Html_data data = _html.front();
      _html.pop();
      _mutex.unlock();

      _consumer_pool.enqueue(Consumer, data, &in_process);
    }

    if (in_process == 0 && _urls.empty() && _html.empty()) {
      break;
    }
  }
  auto finish = std::chrono::steady_clock::now();
  auto time =
      std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
  std::cout << "\nTIME: " << static_cast<double>(time.count()) / 1000
            << std::endl;

  write_in_file();
}

void Parser::Producer(const Url_data& data) {
  auto start = std::chrono::steady_clock::now();

  Html_data html;
  html.depth = data.depth;
  html.html_code = download_page(data.url);

  _mutex.lock();
  _html.push(html);

  auto end = std::chrono::steady_clock::now();
  auto time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << RED << std::this_thread::get_id()
            << " time: " << static_cast<double>(time.count()) / 1000 << STOP
            << std::endl;

  _mutex.unlock();
}

void Parser::Consumer(const Html_data& data, size_t* in_process) {
  auto start = std::chrono::steady_clock::now();

  std::set<std::string> urls = find_url(data.html_code);
  std::set<std::string> temp_pict = find_pictures_urls(data.html_code);

  _mutex.lock();
  for (const auto& it : temp_pict) {
    _picture_urls.insert(it);
  }

  Url_data urls_struct;
  urls_struct.depth = data.depth + 1;

  for (const auto& url : urls) {
    urls_struct.url = url;
    _urls.push(urls_struct);
  }
  (*in_process)--;

  auto end = std::chrono::steady_clock::now();
  auto time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << GREEN << std::this_thread::get_id()
            << " time: " << static_cast<double>(time.count()) / 1000 << STOP
            << std::endl;

  _mutex.unlock();
}

void Parser::write_in_file() {
  std::ofstream out(_filename, std::ios_base::out | std::ios_base::trunc);
  if (out.is_open()) {
    size_t i = 1;
    for (const auto& current_url : _picture_urls) {
      out << std::setw(3) << i++ << ") " << current_url << std::endl;
    }
  } else {
    std::cout << RED << "Site was parsed successfully, but file does not exist"
              << STOP << std::endl;
  }
  out.close();
}

std::string Parser::download_http(const std::string& target) {
  boost::asio::io_context context;
  ip::tcp::resolver resolver{context};
  ip::tcp::socket socket{context};

  auto const result = resolver.resolve(_host, _port);

  boost::asio::connect(socket, result.begin(), result.end());

  http::request<http::string_body> req{http::verb::get, target, 10};
  req.set(http::field::host, _host);
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

std::string Parser::download_https(const std::string& target) {
  // The io_context is required for all I/O
  boost::asio::io_context context;

  // The SSL context is required, and holds certificates
  ssl::context ssl_context{ssl::context::sslv23_client};

  // This holds the root certificate used for verification
  boost::system::error_code ssl_ec;
  load_root_certificates(ssl_context, ssl_ec);
  if (ssl_ec) {
    throw(std::runtime_error(ssl_ec.message()));
  }

  // These objects perform our I/O
  ip::tcp::resolver resolver{context};
  ssl::stream<ip::tcp::socket> stream{context, ssl_context};

  // Look up the domain name
  auto const results = resolver.resolve(_host, _port);

  // Make the connection on the IP address we get from a lookup
  boost::asio::connect(stream.next_layer(), results.begin(), results.end());

  // Perform the SSL handshake
  stream.handshake(ssl::stream_base::client);

  // Set up an HTTP GET request message
  http::request<http::string_body> req{http::verb::get, target, 10};
  req.set(http::field::host, _host);
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

void Parser::gumbo_find_url(GumboNode* node, std::string& parent_url,
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
      gumbo_find_url(static_cast<GumboNode*>(children->data[i]), current_url,
                     urls);
    }
  }
}

std::set<std::string> Parser::find_url(const std::string& html) {
  GumboOutput* outPut = gumbo_parse(html.c_str());
  std::set<std::string> urls;

  std::string parent_url = _type + "://" + _host;
  gumbo_find_url(outPut->root, parent_url, urls);

  return urls;
}

void Parser::gumbo_find_pictures_urls(GumboNode* node, std::string& parent_url,
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
      gumbo_find_pictures_urls(static_cast<GumboNode*>(children->data[i]),
                               parent_url, urls);
    }
  }
}

std::set<std::string> Parser::find_pictures_urls(const std::string& html) {
  GumboOutput* outPut = gumbo_parse(html.c_str());
  std::set<std::string> urls;
  std::string parent_url = _type + "://" + _host;
  gumbo_find_pictures_urls(outPut->root, parent_url, urls);
  return urls;
}

std::string Parser::download_page(const std::string& url) {
  std::string target = find_target(url);
  if (_type == "https") {
    return download_https(target);
  } else if (_type == "http") {
    return download_http(target);
  } else {
    std::cout << "Start address error: " << url << std::endl;
    exit(1);
  }
}

std::string Parser::find_target(std::string url) {
  std::string type = url.substr(0, 5);
  auto protocol_len = 7;
  auto len = url.length();
  if (_type == "https") {
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
