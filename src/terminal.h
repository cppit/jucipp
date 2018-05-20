#pragma once
#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <iostream>
#include "process.hpp"
#include "dispatcher.h"
#include <tuple>

class Terminal : public Gtk::TextView {
  Terminal();
public:
  static Terminal &get() {
    static Terminal singleton;
    return singleton;
  }
  
  int process(const std::string &command, const boost::filesystem::path &path="", bool use_pipes=true);
  int process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path="", std::ostream *stderr_stream=nullptr);
  void async_process(const std::string &command, const boost::filesystem::path &path="", const std::function<void(int exit_status)> &callback=nullptr, bool quiet=false);
  void kill_last_async_process(bool force=false);
  void kill_async_processes(bool force=false);
  
  size_t print(const std::string &message, bool bold=false);
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
  
  std::tuple<size_t, size_t, std::string, std::string, std::string> find_link(const std::string &line);
  void apply_link_tags(const Gtk::TextIter &start_iter, const Gtk::TextIter &end_iter);

  std::vector<std::shared_ptr<TinyProcessLib::Process>> processes;
  std::mutex processes_mutex;
  Glib::ustring stdin_buffer;
};
