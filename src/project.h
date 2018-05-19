#pragma once
#include <gtkmm.h>
#include <boost/filesystem.hpp>
#include <atomic>
#include <unordered_map>
#include "tooltips.h"
#include "dispatcher.h"
#include <iostream>
#include "project_build.h"

namespace Project {
  class DebugRunArguments {
  public:
    std::string arguments;
    bool remote_enabled;
    std::string remote_host_port;
  };
  
  class DebugOptions : public Gtk::Popover {
  public:
    DebugOptions() : Gtk::Popover(), vbox(Gtk::Orientation::ORIENTATION_VERTICAL) { add(vbox); }
    Gtk::Box vbox;
  };
  
  Gtk::Label &debug_status_label();
  void save_files(const boost::filesystem::path &path);
  void on_save(size_t index);
  
  extern boost::filesystem::path debug_last_stop_file_path;
  extern std::unordered_map<std::string, std::string> run_arguments;
  extern std::unordered_map<std::string, DebugRunArguments> debug_run_arguments;
  extern std::atomic<bool> compiling;
  extern std::atomic<bool> debugging;
  extern std::pair<boost::filesystem::path, std::pair<int, int> > debug_stop;
  extern std::string debug_status;
  void debug_update_status(const std::string &new_debug_status);
  void debug_activate_menu_items();
  void debug_update_stop();
  
  class Base : public std::enable_shared_from_this<Base> {
  protected:
    static std::unique_ptr<DebugOptions> debug_options;
  public:
    Base() = default;
    Base(std::unique_ptr<Build> &&build): build(std::move(build)) {}
    virtual ~Base() = default;
    
    std::unique_ptr<Build> build;
    
    Dispatcher dispatcher;
    
    virtual std::pair<std::string, std::string> get_run_arguments();
    virtual void compile();
    virtual void compile_and_run();
    virtual void recreate_build();
    
    virtual void show_symbols();
    
    virtual std::pair<std::string, std::string> debug_get_run_arguments();
    virtual Project::DebugOptions *debug_get_options() { return nullptr; }
    Tooltips debug_variable_tooltips;
    virtual void debug_start();
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
    virtual void debug_cancel() {}
  };
  
  class LLDB : public virtual Base {
  public:
    ~LLDB() override { dispatcher.disconnect(); }
    
#ifdef JUCI_ENABLE_DEBUG
    std::pair<std::string, std::string> debug_get_run_arguments() override;
    Project::DebugOptions *debug_get_options() override;
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
    void debug_cancel() override;
#endif
  };
  
  class LanguageProtocol : public virtual Base {
  public:
    virtual std::string get_language_id()=0;
    void show_symbols() override;
  };
  
  class Clang : public LLDB {
  public:
    Clang(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    std::pair<std::string, std::string> get_run_arguments() override;
    void compile() override;
    void compile_and_run() override;
    void recreate_build() override;    
  };
  
  class Markdown : public Base {
  public:
    Markdown(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    ~Markdown() override;
    
    boost::filesystem::path last_temp_path;
    void compile_and_run() override;
  };
  
  class Python : public LanguageProtocol {
  public:
    Python(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
    
    std::string get_language_id() override { return "python"; }
  };
  
  class JavaScript : public LanguageProtocol {
  public:
    JavaScript(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
    
    std::string get_language_id() override { return "javascript"; }
  };
  
  class HTML : public Base {
  public:
    HTML(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class Rust : public LLDB, public LanguageProtocol {
  public:
    Rust(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    std::pair<std::string, std::string> get_run_arguments() override;
    void compile() override;
    void compile_and_run() override;
    
    std::string get_language_id() override { return "rust"; }
  };
  
  std::shared_ptr<Base> create();
  extern std::shared_ptr<Base> current;
};
