#include "dispatcher.h"

Dispatcher::Dispatcher() {
  connection=dispatcher.connect([this] {
    functions_mutex.lock();
    for(auto &function: functions) {
      function();
    }
    functions.clear();
    functions_mutex.unlock();
  });
}

Dispatcher::~Dispatcher() {
  disconnect();
  functions_mutex.lock();
  functions.clear();
  functions_mutex.unlock();
}

void Dispatcher::push(std::function<void()> &&function) {
  functions_mutex.lock();
  functions.emplace_back(function);
  functions_mutex.unlock();
  dispatcher();
}

void Dispatcher::disconnect() {
  connection.disconnect();
}
