#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_
#include <vector>
#include <string>
#include <boost/filesystem.hpp>

namespace juci {
  class filesystem {
  public:
    static std::string open(std::string);
    static std::string open(boost::filesystem::path path) { return open(path.string()); }
    static std::vector<std::string> lines(std::string);
    static std::vector<std::string> lines(boost::filesystem::path path) { return lines(path.string()); };
    static int save(std::string, std::string);
    static int save(boost::filesystem::path path, std::string new_content) { return save(path.string(), new_content); }
    static int save(std::string path) { return save(path, ""); };
    static int save(boost::filesystem::path path) { return save(path, ""); };
  };
} // namepace juci
#endif  // JUCI_SOURCEFILE_H_
