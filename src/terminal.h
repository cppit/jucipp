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
  class Config {
  public:
    std::vector<std::string> compile_commands;
    std::string run_command;
  }; 
  
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
  bool execute(const std::string &path, const std::string &command);
  void async_execute(const std::string &path, const std::string &command);
  void set_change_folder_command(boost::filesystem::path CMake_path); //TODO: remove
  void run(std::string executable); //TODO: remove
  void compile(); //TODO: remove
  int print(std::string message);
  void print(int line_nr, std::string message);
  std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
private:
  void execute_command(std::string command, std::string mode); //TODO: remove
  
  Gtk::TextView text_view;
  Gtk::ScrolledWindow scrolled_window;
  
  std::string change_folder_command; //TODO: remove
  std::string path; //TODO: remove
  const std::string cmake_sucsess = "Build files have been written to:"; //TODO: remove
  const std::string make_built = "Built target"; //TODO: remove
  const std::string make_executable = "Linking CXX executable"; //TODO: remove
};

#endif  // JUCI_TERMINAL_H_
