#include "dialogs.h"
#include <windows.h>
#include <shlobj.h>

std::string Dialog::select_folder() {
  char selected_folder[MAX_PATH];
  char init_path[MAX_PATH];
  auto title = "Please select a folder";
  selected_folder[0] = 0;
  init_path[0] = 0;
  
  BROWSEINFOA browse;
  browse.hwndOwner = 0;
  browse.pidlRoot = NULL;
  browse.lpszTitle = title;
  browse.pszDisplayName = init_path;
  browse.ulFlags = 0;
  browse.lpfn = NULL;
  auto result = SHBrowseForFolder(&browse);
  
  if(result)
     SHGetPathFromIDListA(result, selected_folder);
   
   std::string dir(selected_folder);
   return dir;
}
/* 
std::string Dialog::new_file() {
  return open_dialog("Please create a new file",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL), std::make_pair("Save", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_SAVE);
}

std::string Dialog::new_folder() {
  return open_dialog("Please create a new folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Create", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER);
}

std::string Dialog::select_file() {
  return open_dialog("Please choose a folder",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Select", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_OPEN);
}

std::string Dialog::save_file() {
  return open_dialog("Please choose a file",
            {std::make_pair("Cancel", Gtk::RESPONSE_CANCEL),std::make_pair("Save", Gtk::RESPONSE_OK)},
            Gtk::FILE_CHOOSER_ACTION_OPEN);
}

*/