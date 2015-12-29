#ifndef JUCI_DEBUG_H_
#define JUCI_DEBUG_H_

#include <boost/filesystem.hpp>
#include <unordered_map>
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBProcess.h"

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
             std::function<void(const boost::filesystem::path &file, int line)> stop_callback=nullptr);
  void continue_debug(); //can't use continue as function name
  void stop();
  void kill();
  
  std::string get_value(const std::string &variable);
  
private:
  lldb::SBDebugger debugger;
  std::unique_ptr<lldb::SBProcess> process;
  std::atomic<bool> stopped;
};

#endif
