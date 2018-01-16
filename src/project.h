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
  Gtk::Label &debug_status_label();
  void save_files(const boost::filesystem::path &path);
  void on_save(size_t index);
  
  extern boost::filesystem::path debug_last_stop_file_path;
  extern std::unordered_map<std::string, std::string> run_arguments;
  extern std::unordered_map<std::string, std::string> debug_run_arguments;
  extern std::atomic<bool> compiling;
  extern std::atomic<bool> debugging;
  extern std::pair<boost::filesystem::path, std::pair<int, int> > debug_stop;
  extern std::string debug_status;
  void debug_update_status(const std::string &new_debug_status);
  void debug_activate_menu_items();
  void debug_update_stop();
  
  class Base : public std::enable_shared_from_this<Base> {
  protected:
#ifdef JUCI_ENABLE_DEBUG
    class DebugOptions : public Gtk::Popover {
    public:
      DebugOptions() : Gtk::Popover(), vbox(Gtk::Orientation::ORIENTATION_VERTICAL) { add(vbox); }
    protected:
      Gtk::Box vbox;
    };
#endif
  public:
    Base(std::unique_ptr<Build> &&build): build(std::move(build)) {}
    virtual ~Base() {}
    
    std::unique_ptr<Build> build;
    
    Dispatcher dispatcher;
    
    virtual std::pair<std::string, std::string> get_run_arguments();
    virtual void compile();
    virtual void compile_and_run();
    virtual void recreate_build();
    
    virtual std::pair<std::string, std::string> debug_get_run_arguments();
    virtual Gtk::Popover *debug_get_options() { return nullptr; }
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
  
  class LLDB : public Base {
#ifdef JUCI_ENABLE_DEBUG
    class DebugOptions : public Base::DebugOptions {
    public:
      DebugOptions();
      Gtk::CheckButton remote_enabled;
      Gtk::Entry remote_host;
    };
    static std::unordered_map<std::string, DebugOptions> debug_options;
#endif
    
  public:
    LLDB(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    ~LLDB() { dispatcher.disconnect(); }
    
#ifdef JUCI_ENABLE_DEBUG
    std::pair<std::string, std::string> debug_get_run_arguments() override;
    Gtk::Popover *debug_get_options() override;
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
  
  class Clang : public LLDB {
  public:
    Clang(std::unique_ptr<Build> &&build) : LLDB(std::move(build)) {}
    
    std::pair<std::string, std::string> get_run_arguments() override;
    void compile() override;
    void compile_and_run() override;
    void recreate_build() override;    
  };
  
  class Markdown : public Base {
  public:
    Markdown(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    ~Markdown();
    
    boost::filesystem::path last_temp_path;
    void compile_and_run() override;
  };
  
  class Python : public Base {
  public:
    Python(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class JavaScript : public Base {
  public:
    JavaScript(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class HTML : public Base {
  public:
    HTML(std::unique_ptr<Build> &&build) : Base(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  class Rust : public LLDB {
  public:
    Rust(std::unique_ptr<Build> &&build) : LLDB(std::move(build)) {}
    
    void compile_and_run() override;
  };
  
  std::shared_ptr<Base> create();
  extern std::shared_ptr<Base> current;
};
