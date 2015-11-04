#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>
#include <boost/filesystem.hpp>

class Dialog {
public:
  static std::string open_folder();
  static std::string open_file();
  static std::string new_file();
  static std::string new_folder();
  static std::string save_file_as(const boost::filesystem::path &file_path);
};

#endif //JUCI_DIALOG_H_
