#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <atomic>
#include <iostream>
#include "process.hpp"

class Terminal : public Gtk::TextView {
public:
  class InProgress {
  public:
    InProgress(const std::string& start_msg);
    ~InProgress();
    void done(const std::string& msg);
    void cancel(const std::string& msg);
  private:
    void start(const std::string& msg);
    size_t line_nr;
    std::atomic<bool> stop;
    Glib::Dispatcher waiting_print;
    std::thread wait_thread;
  };
  
  Terminal();
  int process(const std::string &command, const boost::filesystem::path &path="", bool use_pipes=true);
  int process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path="");
  void async_process(const std::string &command, const boost::filesystem::path &path="", std::function<void(int exit_status)> callback=nullptr);
  void kill_last_async_process(bool force=false);
  void kill_async_processes(bool force=false);
  
  size_t print(const std::string &message, bool bold=false);
  void print(size_t line_nr, const std::string &message);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
  void async_print(const std::string &message, bool bold=false);
  void async_print(int line_nr, const std::string &message);
protected:
  bool on_key_press_event(GdkEventKey *event);
private:
  Glib::Dispatcher async_print_dispatcher;
  Glib::Dispatcher async_print_on_line_dispatcher;
  std::vector<std::pair<std::string, bool> > async_print_strings;
  std::vector<std::pair<int, std::string> > async_print_on_line_strings;
  std::mutex async_print_strings_mutex;
  std::mutex async_print_on_line_strings_mutex;
  Glib::RefPtr<Gtk::TextTag> bold_tag;

  std::vector<std::shared_ptr<Process> > processes;
  std::mutex processes_mutex;
  std::string stdin_buffer;
  
  size_t deleted_lines=0;
};

#endif  // JUCI_TERMINAL_H_
