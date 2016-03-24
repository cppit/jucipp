#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>
#include <boost/filesystem.hpp>
#include <vector>
#include <gtkmm.h>

class Dialog {
public:
  static std::string open_folder(const boost::filesystem::path &path);
  static std::string open_file(const boost::filesystem::path &path);
  static std::string new_file(const boost::filesystem::path &path);
  static std::string new_folder(const boost::filesystem::path &path);
  static std::string save_file_as(const boost::filesystem::path &path);
  
  class Message : public Gtk::MessageDialog {
  public:
    Message(const std::string &text);
  };
  
private:
  static std::string gtk_dialog(const boost::filesystem::path &path, const std::string &title,
                          const std::vector<std::pair<std::string, Gtk::ResponseType>> &buttons,
                          Gtk::FileChooserAction gtk_options);
};

#endif //JUCI_DIALOG_H_
