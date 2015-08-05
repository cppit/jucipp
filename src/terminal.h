#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <atomic>

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
  bool execute(const std::string &command, const std::string &path="");
  void async_execute(const std::string &command, const std::string &path="", std::function<void(bool success)> callback=nullptr);
  int print(std::string message);
  void print(int line_nr, std::string message);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
private:
  Gtk::TextView text_view;
  Gtk::ScrolledWindow scrolled_window;

  Glib::Dispatcher async_execute_print;
  std::string async_execute_print_string;
  std::atomic<bool> async_execute_print_finished;
};

#endif  // JUCI_TERMINAL_H_
