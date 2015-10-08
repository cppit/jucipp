#ifdef _WIN32
#include "dialogs.h"
#include <c_dialogs.h>

std::string Dialog::select_folder() {
  return c_select_folder();
}

std::string Dialog::new_file() {
  return c_new_file();
}

std::string Dialog::new_folder() {
  return c_new_folder();
}

std::string Dialog::select_file() {
  return c_select_file();
}

std::string Dialog::save_file() {
  return c_save_file();
}
#endif