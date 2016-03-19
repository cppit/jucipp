#ifndef JUCI_CMAKE_H_
#define JUCI_CMAKE_H_
#include <boost/filesystem.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class CMake {
public:
  CMake(const boost::filesystem::path &path);
  boost::filesystem::path project_path;
  std::vector<boost::filesystem::path> paths;
  
  bool update_default_build(const boost::filesystem::path &default_build_path, bool force=false);
  bool update_debug_build(const boost::filesystem::path &debug_build_path, bool force=false);
  
  boost::filesystem::path get_executable(const boost::filesystem::path &file_path);
  
  std::vector<std::pair<boost::filesystem::path, std::vector<std::string> > > get_functions_parameters(const std::string &name);
private:
  std::vector<std::string> files;
  std::unordered_map<std::string, std::string> variables;
  void read_files();
  void remove_tabs();
  void remove_comments();
  void remove_newlines_inside_parentheses();
  void find_variables();
  void parse_variable_parameters(std::string &data);
  void parse();
  std::vector<std::string> get_function_parameters(std::string &data);
  bool parsed=false;
};
#endif //JUCI_CMAKE_H_
