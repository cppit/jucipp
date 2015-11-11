#include "dialogs.h"

std::string Dialog::open_folder() {
  return gtk_dialog("Open Folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Open", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

std::string Dialog::new_file() {
  return gtk_dialog("New File",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL), std::make_pair("Save", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SAVE);
}

std::string Dialog::new_folder() {
  return gtk_dialog("New Folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Create", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER);
}

std::string Dialog::open_file() {
  return gtk_dialog("Open File",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Select", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_OPEN);
}

std::string Dialog::save_file_as(const boost::filesystem::path &file_path) {
  return gtk_dialog("Save File As",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Save", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SAVE, file_path.string());
}

