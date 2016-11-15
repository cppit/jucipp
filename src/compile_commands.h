#ifndef JUCI_COMPILE_COMMANDS_H_
#define JUCI_COMPILE_COMMANDS_H_

#include <boost/filesystem.hpp>
#include <vector>
#include <string>

class CompileCommands {
public:
  class Command {
  public:
    boost::filesystem::path directory;
    std::vector<std::string> parameters;
    boost::filesystem::path file;
    
    std::vector<std::string> paramter_values(const std::string &parameter_name) const;
  };
  
  CompileCommands(const boost::filesystem::path &build_path);
  std::vector<Command> commands;
};

#endif // JUCI_COMPILE_COMMANDS_H_
