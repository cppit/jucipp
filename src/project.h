#ifndef JUCI_PROJECT_H_
#define JUCI_PROJECT_H_

#include "notebook.h"
#include "cmake.h"
#include <boost/filesystem.hpp>
#include "directories.h"
#include <atomic>
#include <mutex>
#include "tooltips.h"

class Project {
public:
  Project(Notebook &notebook) : notebook(notebook) {}
  virtual ~Project() {}
  
  Notebook &notebook;
  
  static std::unordered_map<std::string, std::string> run_arguments;
  static std::unordered_map<std::string, std::string> debug_run_arguments;
  static std::atomic<bool> compiling;
  static std::atomic<bool> debugging;
  
  virtual std::pair<std::string, std::string> get_run_arguments() {return {"", ""};}
  virtual void compile() {}
  virtual void compile_and_run() {}
  
  virtual std::pair<std::string, std::string> debug_get_run_arguments() {return {"", ""};}
  Tooltips debug_variable_tooltips;
  virtual void debug_start(std::function<void(const std::string &status)> status_callback,
                           std::function<void(const boost::filesystem::path &file_path, int line_nr, int line_index)> stop_callback) {}
  virtual void debug_continue() {}
  virtual void debug_stop() {}
  virtual void debug_kill() {}
  virtual void debug_step_over() {}
  virtual void debug_step_into() {}
  virtual void debug_step_out() {}
  virtual void debug_backtrace() {}
  virtual void debug_show_variables() {}
  virtual void debug_run_command(const std::string &command) {}
  virtual void debug_delete() {}
};

class ProjectClang : public Project {
public:
  ProjectClang(Notebook &notebook) : Project(notebook) {}
  
  std::unique_ptr<CMake> get_cmake();
  
  std::pair<std::string, std::string> get_run_arguments() override;
  void compile() override;
  void compile_and_run() override;
  
  std::mutex debug_start_mutex;
#ifdef JUCI_ENABLE_DEBUG
  std::pair<std::string, std::string> debug_get_run_arguments() override;
  void debug_start(std::function<void(const std::string &status)> status_callback,
                   std::function<void(const boost::filesystem::path &file_path, int line_nr, int line_index)> stop_callback) override;
  void debug_continue() override;
  void debug_stop() override;
  void debug_kill() override;
  void debug_step_over() override;
  void debug_step_into() override;
  void debug_step_out() override;
  void debug_backtrace() override;
  void debug_show_variables() override;
  void debug_run_command(const std::string &command) override;
  void debug_delete() override;
#endif
};

class ProjectMarkdown : public Project {
public:
  ProjectMarkdown(Notebook &notebook) : Project(notebook) {}
  ~ProjectMarkdown();
  
  boost::filesystem::path last_temp_path;
  void compile_and_run() override;
};

class ProjectPython : public Project {
public:
  ProjectPython(Notebook &notebook) : Project(notebook) {}
  
  void compile_and_run() override;
};

class ProjectJavaScript : public Project {
public:
  ProjectJavaScript(Notebook &notebook) : Project(notebook) {}
  
  void compile_and_run() override;
};

class ProjectHTML : public Project {
public:
  ProjectHTML(Notebook &notebook) : Project(notebook) {}
  
  void compile_and_run() override;
};

#endif  // JUCI_PROJECT_H_
