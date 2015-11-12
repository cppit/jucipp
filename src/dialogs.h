#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>
#include <boost/filesystem.hpp>
#include <vector>
#include <gtkmm.h>

class Dialog {
public:
  static std::string open_folder();
  static std::string open_file();
  static std::string new_file();
  static std::string new_folder();
  static std::string save_file_as(const boost::filesystem::path &file_path);
  
  class Message : public Gtk::MessageDialog {
  public:
    Message(const std::string &text);
  };
  
private:
  static std::string gtk_dialog(const std::string &title,
                          const std::vector<std::pair<std::string, Gtk::ResponseType>> &buttons,
                          Gtk::FileChooserAction gtk_options,
                          const std::string &file_name = "");
};

#endif //JUCI_DIALOG_H_
