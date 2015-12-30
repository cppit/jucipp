#ifndef JUCI_DEBUG_H_
#define JUCI_DEBUG_H_

#include <boost/filesystem.hpp>
#include <unordered_map>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBListener.h>
#include <lldb/API/SBProcess.h>
#include <thread>
#include <mutex>

class Debug {
private:
  Debug();
public:
  static Debug &get() {
    static Debug singleton;
    return singleton;
  }
  
  void start(std::shared_ptr<std::vector<std::pair<boost::filesystem::path, int> > > breakpoints,
             const boost::filesystem::path &executable, const boost::filesystem::path &path="",
             std::function<void(int exit_status)> callback=nullptr,
             std::function<void(const std::string &status)> status_callback=nullptr,
             std::function<void(const boost::filesystem::path &file_path, int line_nr, int line_index)> stop_callback=nullptr);
  void continue_debug(); //can't use continue as function name
  void stop();
  void kill();
  std::pair<std::string, std::string> run_command(const std::string &command);
  
  void delete_debug(); //can't use delete as function name
  
  std::string get_value(const std::string &variable, const boost::filesystem::path &file_path, unsigned int line_nr);
    
private:
  lldb::SBDebugger debugger;
  lldb::SBListener listener;
  std::unique_ptr<lldb::SBProcess> process;
  std::thread debug_thread;
  
  lldb::StateType state;
  std::mutex event_mutex;
  
  size_t buffer_size;
};

#endif
