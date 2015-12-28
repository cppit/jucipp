#ifndef JUCI_DEBUG_H_
#define JUCI_DEBUG_H_

#include <boost/filesystem.hpp>
#include <unordered_map>
#include "lldb/API/SBDebugger.h"

class Debug {
private:
  Debug();
public:
  static Debug &get() {
    static Debug singleton;
    return singleton;
  }
  
  void start(const boost::filesystem::path &project_path, const boost::filesystem::path &executable, const boost::filesystem::path &path="", std::function<void(int exit_status)> callback=nullptr);
  
  std::unordered_map<std::string, std::vector<std::pair<std::string, int> > > breakpoints;
  
private:
  lldb::SBDebugger debugger;
};

#endif
