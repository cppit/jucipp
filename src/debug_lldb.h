#pragma once
#include <boost/filesystem.hpp>
#include <list>
#include <lldb/API/LLDB.h>
#include <thread>
#include <mutex>
#include <tuple>

namespace Debug {
  class LLDB {
  public:
    class Frame {
    public:
      uint32_t index;
      std::string module_filename;
      boost::filesystem::path file_path;
      std::string function_name;
      int line_nr;
      int line_index;
    };
    class Variable {
    public:
      uint32_t thread_index_id;
      uint32_t frame_index;
      std::string name;
      std::string value;
      bool declaration_found;
      boost::filesystem::path file_path;
      int line_nr;
      int line_index;
    };
  private:
    LLDB();
  public:
    static LLDB &get() {
      static LLDB singleton;
      return singleton;
    }
    
    std::list<std::function<void(const lldb::SBProcess &)>> on_start;
    /// The handlers are not run in the main loop.
    std::list<std::function<void(int exit_status)>> on_exit;
    /// The handlers are not run in the main loop.
    std::list<std::function<void(const lldb::SBEvent &)>> on_event;
    
    std::mutex mutex;

    void start(const std::string &command, const boost::filesystem::path &path="",
               const std::vector<std::pair<boost::filesystem::path, int> > &breakpoints={},
               const std::vector<std::string> &startup_commands={}, const std::string &remote_host="");
    void continue_debug(); //can't use continue as function name
    void stop();
    void kill();
    void step_over();
    void step_into();
    void step_out();
    std::pair<std::string, std::string> run_command(const std::string &command);
    std::vector<Frame> get_backtrace();
    std::vector<Variable> get_variables();
    void select_frame(uint32_t frame_index, uint32_t thread_index_id=0);
    
    void cancel();
    
    std::string get_value(const std::string &variable, const boost::filesystem::path &file_path, unsigned int line_nr, unsigned int line_index);
    std::string get_return_value(const boost::filesystem::path &file_path, unsigned int line_nr, unsigned int line_index);
    
    bool is_invalid();
    bool is_stopped();
    bool is_running();
    
    void add_breakpoint(const boost::filesystem::path &file_path, int line_nr);
    void remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count);
    
    void write(const std::string &buffer);
    
  private:
    std::tuple<std::vector<std::string>, std::string, std::vector<std::string>> parse_run_arguments(const std::string &command);
    
    std::unique_ptr<lldb::SBDebugger> debugger;
    std::unique_ptr<lldb::SBListener> listener;
    std::unique_ptr<lldb::SBProcess> process;
    std::thread debug_thread;
    
    lldb::StateType state;
    
    size_t buffer_size;
  };
}
