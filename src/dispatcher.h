#ifndef DISPATCHER_H_
#define	DISPATCHER_H_
#include <gtkmm.h>
#include <mutex>
#include <vector>

class Dispatcher {
private:
  std::vector<std::function<void()>> functions;
  std::mutex functions_mutex;
  Glib::Dispatcher dispatcher;
  sigc::connection connection;
public:
  Dispatcher();
  ~Dispatcher();
  void post(std::function<void()> &&function);
  void disconnect();
};

#endif	/* DISPATCHER_H_ */
