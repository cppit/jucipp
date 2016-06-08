#include "config.h"

Config::Config() {
  terminal.lldb_command="lldb";
#ifdef __linux
  if(terminal.lldb_command=="lldb" &&
     !boost::filesystem::exists("/usr/bin/lldb") && !boost::filesystem::exists("/usr/local/bin/lldb")) {
    if(boost::filesystem::exists("/usr/bin/lldb-3.9"))
      terminal.lldb_command="/usr/bin/lldb-3.9";
    else if(boost::filesystem::exists("/usr/bin/lldb-3.8"))
      terminal.lldb_command="/usr/bin/lldb-3.8";
    else if(boost::filesystem::exists("/usr/bin/lldb-3.7"))
      terminal.lldb_command="/usr/bin/lldb-3.7";
    else if(boost::filesystem::exists("/usr/bin/lldb-3.6"))
      terminal.lldb_command="/usr/bin/lldb-3.6";
    else if(boost::filesystem::exists("/usr/bin/lldb-3.5"))
      terminal.lldb_command="/usr/bin/lldb-3.5";
  }
#endif
}
