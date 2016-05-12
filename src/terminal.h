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
#include "dispatcher.h"
#include <unordered_set>

class Terminal : public Gtk::TextView {
public:
  class InProgress {
    friend class Terminal;
  public:
    InProgress(const std::string& start_msg);
    ~InProgress();
    void done(const std::string& msg);
    void cancel(const std::string& msg);
  private:
    void start(const std::string& msg);
    size_t line_nr;
    
    std::atomic<bool> stop;
    
    std::thread wait_thread;
  };
  
private:
  Terminal();
public:
  static Terminal &get() {
    static Terminal singleton;
    return singleton;
  }
  
  int process(const std::string &command, const boost::filesystem::path &path="", bool use_pipes=true);
  int process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path="");
  void async_process(const std::string &command, const boost::filesystem::path &path="", std::function<void(int exit_status)> callback=nullptr);
  void kill_last_async_process(bool force=false);
  void kill_async_processes(bool force=false);
  
  size_t print(const std::string &message, bool bold=false);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
  void async_print(const std::string &message, bool bold=false);
  void async_print(size_t line_nr, const std::string &message);
  
  void configure();
  
  void clear();
protected:
  bool on_motion_notify_event (GdkEventMotion* motion_event) override;
  bool on_button_press_event(GdkEventButton* button_event) override;
  bool on_key_press_event(GdkEventKey *event) override;
private:
  Dispatcher dispatcher;
  Glib::RefPtr<Gtk::TextTag> bold_tag;
  Glib::RefPtr<Gtk::TextTag> link_tag;
  Glib::RefPtr<Gdk::Cursor> link_mouse_cursor;
  Glib::RefPtr<Gdk::Cursor> default_mouse_cursor;
  size_t deleted_lines=0;
  void apply_link_tags(Gtk::TextIter start_iter, Gtk::TextIter end_iter);

  std::vector<std::shared_ptr<Process> > processes;
  std::mutex processes_mutex;
  std::string stdin_buffer;
  
  std::unordered_set<InProgress*> in_progresses;
  std::mutex in_progresses_mutex;
};

#endif  // JUCI_TERMINAL_H_
