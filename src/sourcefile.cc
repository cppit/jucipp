#include "sourcefile.h"
#include <fstream>

std::string juci::filesystem::open(std::string path) {
  std::string res;
  for (auto &line : lines(path)) {
    res += line+'\n';
  }
  return res;
}

std::vector<std::string> juci::filesystem::lines(std::string path) {
  std::vector<std::string> res;
  std::ifstream input(path);
  if (input.is_open()) {
    do { res.emplace_back(); } while(getline(input, res.back()));
  }
  input.close();
  return res;
}

int juci::filesystem::save(std::string path, std::string new_content) {
  std::ofstream output(path);
  if(output.is_open()) {
    output << new_content;
  } else {
    output.close();
    return 1;
  }
  output.close();
  return 0;
}