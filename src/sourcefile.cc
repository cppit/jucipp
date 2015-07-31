#include "sourcefile.h"
#include <fstream>
#include <sstream>

//TODO: make bool juci::filesystem::open(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> buffer)
//TODO: make bool juci::filesystem::save(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> buffer)
//Only use on small files
std::string juci::filesystem::open(const std::string &path) {
  std::stringstream ss;
  std::ifstream input(path);
  if(input) {
    ss << input.rdbuf();
    input.close();
  }
  return ss.str();
}

//Only use on small files
std::vector<std::string> juci::filesystem::lines(const std::string &path) {
  std::vector<std::string> res;
  std::ifstream input(path);
  if (input) {
    do { res.emplace_back(); } while(getline(input, res.back()));
  }
  input.close();
  return res;
}

//Only use on small files
bool juci::filesystem::save(const std::string &path, const std::string &new_content) {
  std::ofstream output(path);
  if(output)
    output << new_content;
  else
    return false;
  output.close();
  return true;
}