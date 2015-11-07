#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>
#include <boost/filesystem.hpp>
#include <vector>
#include <gtkmm.h>
#include "singletons.h"
#include "juci.h"

class Dialog {
public:
  static std::string open_folder();
  static std::string open_file();
  static std::string new_file();
  static std::string new_folder();
  static std::string save_file_as(const boost::filesystem::path &file_path);
  
private:
  static std::string gtk_dialog(const std::string &title,
                          const std::vector<std::pair<std::string, Gtk::ResponseType>> &buttons,
                          Gtk::FileChooserAction gtk_options,
                          const std::string &file_name = "") {
    Gtk::FileChooserDialog dialog(title, gtk_options);
    if(!Singleton::directories->current_path.empty())
      gtk_file_chooser_set_current_folder((GtkFileChooser*)dialog.gobj(), Singleton::directories->current_path.string().c_str());
    else
      gtk_file_chooser_set_current_folder((GtkFileChooser*)dialog.gobj(), boost::filesystem::current_path().string().c_str());
    if (!file_name.empty())
      gtk_file_chooser_set_filename((GtkFileChooser*)dialog.gobj(), file_name.c_str());
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);
    
    auto g_application=g_application_get_default(); //TODO: Post issue that Gio::Application::get_default should return pointer and not Glib::RefPtr
    auto gio_application=Glib::wrap(g_application, true);
    auto application=Glib::RefPtr<Application>::cast_static(gio_application);
    dialog.set_transient_for(*application->window);
    
    for (auto &button : buttons) 
      dialog.add_button(button.first, button.second);
    return dialog.run() == Gtk::RESPONSE_OK ? dialog.get_filename() : "";
  }
};

#endif //JUCI_DIALOG_H_
