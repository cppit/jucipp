#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>

class Dialog {
  public:
    static std::string select_folder();
    static std::string select_file();
    static std::string new_file();
    static std::string new_folder();
    static std::string save_file();
}; // namespace Dialog

#endif  // JUCI_DIALOG_H_