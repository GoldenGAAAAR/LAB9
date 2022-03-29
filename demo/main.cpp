#include "start_options.hpp"

int main(int argc, const char* argv[]) {
  //"/Users/mihailkoraev/Documents/GitHub/lab-09-producer-consumer/data/urls.txt";

  Options app(argc, argv);
  return app.exec();
}
