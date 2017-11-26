#include "dispatcher.h"

Dispatcher::Dispatcher() {
  connection=dispatcher.connect([this] {
    std::unique_lock<std::mutex> lock(functions_mutex);
    for(auto &function: functions)
      function();
    functions.clear();
  });
}

Dispatcher::~Dispatcher() {
  disconnect();
  std::unique_lock<std::mutex> lock(functions_mutex);
  functions.clear();
}

void Dispatcher::disconnect() {
  connection.disconnect();
}
