#ifndef JUCI_PROJECT_H_
#define JUCI_PROJECT_H_

#include "cmake.h"
#include <boost/filesystem.hpp>
#include "directories.h"
#include <atomic>

class Project {
public:
  Project(const boost::filesystem::path &file_path) : file_path(file_path) {}
  virtual ~Project() {}
  
  boost::filesystem::path file_path;
  
  static std::unordered_map<std::string, std::string> run_arguments;
  static std::unordered_map<std::string, std::string> debug_run_arguments;
  static std::atomic<bool> compiling;
  static std::atomic<bool> debugging;
  
  virtual std::pair<std::string, std::string> get_run_arguments() {return {"", ""};}
  virtual void compile() {}
  virtual void compile_and_run() {}
  
  virtual std::pair<std::string, std::string> debug_get_run_arguments() {return {"", ""};}
  virtual void debug_start_continue() {}
  virtual void debug_stop() {}
  virtual void debug_kill() {}
  virtual void debug_step_over() {}
  virtual void debug_step_into() {}
  virtual void debug_step_out() {}
  virtual void debug_backtrace() {}
  virtual void debug_show_variables() {}
  virtual void debug_run_command() {}
  virtual void debug_toggle_breakpoint() {}
  virtual void debug_goto_stop() {}
};

class ProjectClang : public Project {
public:
  ProjectClang(const boost::filesystem::path &file_path) : Project(file_path) {}
  
  std::unique_ptr<CMake> get_cmake();
  
  std::pair<std::string, std::string> get_run_arguments() override;
  void compile() override;
  void compile_and_run() override;
};

#endif  // JUCI_PROJECT_H_
