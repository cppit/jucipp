#pragma once
#include <gtkmm.h>
#include <mutex>
#include <functional>
#include <list>

class Dispatcher {
private:
  std::list<std::function<void()>> functions;
  std::mutex functions_mutex;
  Glib::Dispatcher dispatcher;
  sigc::connection connection;
public:
  Dispatcher();
  ~Dispatcher();
  
  template<typename T>
  void post(T &&function) {
    {
      std::unique_lock<std::mutex> lock(functions_mutex);
      functions.emplace_back(std::forward<T>(function));
    }
    dispatcher();
  }
  
  void disconnect();
};
