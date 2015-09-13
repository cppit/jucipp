#include "dialogs.h"
#include <shobjidl.h>
#include <memory>
#include <exception>
#include <singletons.h>

#ifndef check(__fun__)
#define MESSAGE "An error occurred when trying open windows dialog"
#define check(__fun__) auto __hr__ = __fun__; if(FAILED(__hr__)) Singleton::terminal()->print(MESSAGE)
#endif


std::string Dialog::select_folder() {
  IFileOpenDialog *dialog = nullptr;
  check(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)));
  DWORD options;
  check(dialog->GetOptions(&options));
  
}
/* 
std::string Dialog::new_file() {
  return open_dialog("Please create a new file",
            {std::make_pair("Cancel", Gtk::RESPONSECANCEL), std::make_pair("Save", Gtk::RESPONSE_OK)},
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