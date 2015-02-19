#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_

#include <string>
#include <vector>

using namespace std;

class sourcefile {
public:
  explicit sourcefile(const string &filename);
  vector<string> get_lines();
  string get_content();
  string get_line(int line_number);
  int save(const string &text);

private:
  void open(const string &filename);
  vector<string> lines;
  string filename;
};

#endif  // JUCI_SOURCEFILE_H_
