#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include <mutex>
#include "gtkmm.h"
#include <boost/filesystem.hpp>

namespace Terminal {

  class Config {
  public:
    Config ();
    Config(Terminal::Config& original);
    std::vector<std::string>& compile_commands() {return &compile_commands_;}
    void InsertCompileCommand(std::string command);
  private:
    std::vector<std::string> compile_commands_;
  };
 
  class View {
  public:
    View();   
    Gtk::HBox& view() {return view_;}
    Gtk::TextView& textview() {return textview_;}
  private:
    Gtk::HBox view_;
    Gtk::TextView textview_;
    Gtk::ScrolledWindow scrolledwindow_;
  };  // class view
  
  class Controller {  
  public:
    Controller(Terminal::Config& cfg);
    Gtk::HBox& view() {return view_.view();}
    Gtk::TextView& Terminal(){return view_.textview();}
    void SetFolderCommand(boost::filesystem::path CMake_path);
    void Run(std::string executable);
    void Compile();
    Terminal::Config& config() { return config_; }
  private:
    Terminal::Config config_;
    void ExecuteCommand(std::string command, std::string mode);
    bool OnButtonRealeaseEvent(GdkEventKey* key);
    bool ExistInConsole(std::string string);
    void PrintMessage(std::string message);
    Terminal::View view_;
    std::string folder_command_;
    std::string path_;
    const std::string cmake_sucsess = "Build files have been written to:";
    const std::string make_built = "Built target";
    const std::string make_executable = "Linking CXX executable";
  };  // class controller
}  // namespace Terminal

#endif  // JUCI_TERMINAL_H_
