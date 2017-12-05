#include "dispatcher.h"
#include <vector>

Dispatcher::Dispatcher() {
  connection=dispatcher.connect([this] {
    std::vector<std::list<std::function<void()>>::iterator> its;
    {
      std::unique_lock<std::mutex> lock(functions_mutex);
      if(functions.empty())
        return;
      its.reserve(functions.size());
      for(auto it=functions.begin();it!=functions.end();++it)
        its.emplace_back(it);
    }
    for(auto &it: its)
      (*it)();
    {
      std::unique_lock<std::mutex> lock(functions_mutex);
      for(auto &it: its)
        functions.erase(it);
    }
  });
}

Dispatcher::~Dispatcher() {
  disconnect();
}

void Dispatcher::disconnect() {
  connection.disconnect();
}
