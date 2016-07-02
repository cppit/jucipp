#ifndef JUCI_CTAGS_H_
#define JUCI_CTAGS_H_
#include <string>
#include <boost/filesystem.hpp>
#include <sstream>
#include <vector>

class Ctags {
public:
  class Data {
  public:
    std::string path;
    unsigned long line;
    unsigned long index;
    std::string source;
  };
  
  static std::unique_ptr<std::stringstream> get_result(const boost::filesystem::path &path, std::vector<boost::filesystem::path> exclude_paths);
  
  static Data parse_line(const std::string &line);
};

#endif //JUCI_CTAGS_H_
