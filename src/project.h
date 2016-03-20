#ifndef JUCI_PROJECT_H_
#define JUCI_PROJECT_H_

#include <gtkmm.h>
#include <boost/filesystem.hpp>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include "tooltips.h"
#include "dispatcher.h"
#include <iostream>
#include "project_build.h"

namespace Project {
  Gtk::Label &debug_status_label();
  void save_files(const boost::filesystem::path &path);
  void on_save(int page);
  
  extern boost::filesystem::path debug_last_stop_file_path;
  extern std::unordered_map<std::string, std::string> run_arguments;
  extern std::unordered_map<std::string, std::string> debug_run_arguments;
  extern std::atomic<bool> compiling;
  extern std::atomic<bool> debugging;
  extern std::pair<boost::filesystem::path, std::pair<int, int> > debug_stop;
  void debug_update_stop();
  void debug_update_status(const std::string &debug_status);
  
  class Language {
  public:
    Language(std::unique_ptr<Build> &&build): build(std::move(build)) {}
    virtual ~Language() {}
    
    std::unique_ptr<Build> build;
    
    virtual std::pair<std::string, std::string> get_run_arguments() {return {"", ""};}
    virtual void compile() {}
    virtual void compile_and_run() {}
    
    virtual std::pair<std::string, std::string> debug_get_run_arguments() {return {"", ""};}
    Tooltips debug_variable_tooltips;
    virtual void debug_start() {}
    virtual void debug_continue() {}
    virtual void debug_stop() {}
    virtual void debug_kill() {}
    virtual void debug_step_over() {}
    virtual void debug_step_into() {}
    virtual void debug_step_out() {}
    virtual void debug_backtrace() {}
    virtual void debug_show_variables() {}
    virtual void debug_run_command(const std::string &command) {}
    virtual void debug_add_breakpoint(const boost::filesystem::path &file_path, int line_nr) {}
    virtual void debug_remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) {}
    virtual bool debug_is_running() { return false; }
    virtual void debug_write(const std::string &buffer) {}
    virtual void debug_delete() {}
  };
  
  class Clang : public Language {
  private:
    Dispatcher dispatcher;
  public:
    Clang(std::unique_ptr<Build> &&build) : Language(std::move(build)) {}
    ~Clang() { dispatcher.disconnect(); }
    
    std::pair<std::string, std::string> get_run_arguments() override;
    void compile() override;
    void compile_and_run() override;
    
    std::mutex debug_start_mutex;
  #ifdef JUCI_ENABLE_DEBUG
    std::pair<std::string, std::string> debug_get_run_arguments() override;
    void debug_start() override;
    void debug_continue() override;
    void debug_stop() override;
    void debug_kill() override;
    void debug_step_over() override;
    void debug_step_into() override;
    void debug_step_out() override;
    void debug_backtrace() override;
    void debug_show_variables() override;
    void debug_run_command(const std::string &command) override;
    void debug_add_breakpoint(const boost::filesystem::path &file_path, int line_nr) override;
    void debug_remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) override;
    bool debug_is_running() override;
    void debug_write(const std::string &buffer) override;
    void debug_delete() override;
  #endif
  };
  
  class Markdown : public Language {
  public:
    Markdown(std::unique_ptr<Build> &&build) : Language(std::move(build)) {}
    ~Markdown();
    
    boost::filesystem::path last_temp_path;
    void compile_and_run() override;
  };
  
  class Python : public Language {
  public:
    Python(std::unique_ptr<Build> &&build) : Language(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class JavaScript : public Language {
  public:
    JavaScript(std::unique_ptr<Build> &&build) : Language(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class HTML : public Language {
  public:
    HTML(std::unique_ptr<Build> &&build) : Language(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  std::unique_ptr<Language> get_language();
  extern std::unique_ptr<Language> current_language;
};

#endif  // JUCI_PROJECT_H_
