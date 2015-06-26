#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include <functional>
#include "gtkmm.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <atomic>

namespace Terminal {
  class Config {
  public:
    std::vector<std::string> compile_commands;
    std::string run_command;
  };
 
  class View : public Gtk::HBox {
  public:
    View();
    Gtk::TextView text_view;
    Gtk::ScrolledWindow scrolled_window;
  };  // class view
  
  class Controller;
  
  //Temporary solution for displaying functions in progress, and when they are done.
  class InProgress {
  public:
    InProgress(Controller& terminal, const std::string& start_msg);
    ~InProgress();
    void done(const std::string& msg);
    void cancel(const std::string& msg);
  private:
    void start();
    Controller& terminal;
    std::string start_msg;
    int line_nr;
    std::atomic<bool> stop;
    Glib::Dispatcher waiting_print;
    std::thread wait_thread;
  };
  
  class Controller {  
  public:
    Controller(Terminal::Config& cfg);
    void SetFolderCommand(boost::filesystem::path CMake_path);
    void Run(std::string executable);
    void Compile();
    int print(std::string message);
    void print(int line_nr, std::string message);
    std::shared_ptr<InProgress> print_in_progress(std::string start_msg);
    Terminal::View view;
  private:
    Terminal::Config& config;
    void ExecuteCommand(std::string command, std::string mode);
    bool OnButtonRealeaseEvent(GdkEventKey* key);
    bool ExistInConsole(std::string string);
    std::string folder_command_;
    std::string path_;
    const std::string cmake_sucsess = "Build files have been written to:";
    const std::string make_built = "Built target";
    const std::string make_executable = "Linking CXX executable";
  };  // class controller
}  // namespace Terminal

#endif  // JUCI_TERMINAL_H_
