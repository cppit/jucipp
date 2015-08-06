#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <atomic>
#include <unordered_map>

class Terminal : public Gtk::HBox {
public:  
  class InProgress {
  public:
    InProgress(const std::string& start_msg);
    ~InProgress();
    void done(const std::string& msg);
    void cancel(const std::string& msg);
  private:
    void start(const std::string& msg);
    int line_nr;
    std::atomic<bool> stop;
    Glib::Dispatcher waiting_print;
    std::thread wait_thread;
  };
  
  Terminal();
  int execute(const std::string &command, const std::string &path="");
  void async_execute(const std::string &command, const std::string &path="", std::function<void(int exit_code)> callback=nullptr);
  std::unordered_map<pid_t, std::pair<int, int> > async_pid_descriptors;
  std::unordered_map<pid_t, int> async_pid_status;
  std::mutex async_pid_mutex;
  
  int print(const std::string &message);
  void print(int line_nr, const std::string &message);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
  void async_print(const std::string &message);
private:
  Gtk::TextView text_view;
  Gtk::ScrolledWindow scrolled_window;

  Glib::Dispatcher async_print_dispatcher;
  std::string async_print_string;
  std::mutex async_print_string_mutex;
};

#endif  // JUCI_TERMINAL_H_
