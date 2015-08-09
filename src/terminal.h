#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <atomic>
#include <list>

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
    int line_nr;
    std::atomic<bool> stop;
    Glib::Dispatcher waiting_print;
    std::thread wait_thread;
  };
  
  Terminal();
  int execute(const std::string &command, const boost::filesystem::path &path="");
  void async_execute(const std::string &command, const boost::filesystem::path &path="", std::function<void(int exit_code)> callback=nullptr);
  void kill_last_async_execute(bool force=false);
  void kill_async_executes(bool force=false);
  
  int print(const std::string &message, bool bold=false);
  void print(int line_nr, const std::string &message, bool bold=false);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
  void async_print(const std::string &message, bool bold=false);
protected:
  bool on_key_press_event(GdkEventKey *event);
private:
  Glib::Dispatcher async_print_dispatcher;
  std::vector<std::pair<std::string, bool> > async_print_strings;
  std::mutex async_print_strings_mutex;
  Glib::RefPtr<Gtk::TextTag> bold_tag;

  std::mutex async_executes_mutex;
  std::list<std::pair<pid_t, int> > async_executes;
  
  std::string stdin_buffer;
};

#endif  // JUCI_TERMINAL_H_
