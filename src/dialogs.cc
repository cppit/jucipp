#include "dialogs.h"
#include "singletons.h"
#include <gtkmm.h>
#include <vector>
#include "juci.h"
#include <iostream>

std::string open_dialog(const std::string &title,
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

std::string Dialog::open_folder() {
  return open_dialog("Open Folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Open", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

std::string Dialog::new_file() {
  return open_dialog("New File",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL), std::make_pair("Save", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SAVE);
}

std::string Dialog::new_folder() {
  return open_dialog("New Folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Create", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER);
}

std::string Dialog::open_file() {
  return open_dialog("Open File",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Select", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_OPEN);
}

std::string Dialog::save_file_as(const boost::filesystem::path &file_path) {
  return open_dialog("Save File As",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Save", Gtk::RESPONSE_OK)},
                     Gtk::FILE_CHOOSER_ACTION_SAVE, file_path.string());
}

