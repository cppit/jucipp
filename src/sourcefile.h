#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_

#include <string>
#include <vector>

class sourcefile {
public:
  explicit sourcefile(const std::string &filename);
  std::vector<std::string> get_lines();
  std::string get_content();
  std::string get_line(int line_number);
  int save(const std::string &text);

private:
  void open(const std::string &filename);
  std::vector<std::string> lines;
  std::string filename;
};

#endif  // JUCI_SOURCEFILE_H_
