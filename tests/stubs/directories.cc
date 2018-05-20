#include "directories.h"

Directories::Directories() : ListViewText(1) {}

Directories::~Directories() {}

void Directories::on_save_file(const boost::filesystem::path &file_path) {}

bool Directories::on_button_press_event(GdkEventButton *event) {
  return false;
};
