#include "notebook.h"

Notebook::Notebook() {}

Source::View *Notebook::get_current_view() {
  return nullptr;
}

void Notebook::open(const boost::filesystem::path &file_path, size_t notebook_index) {}
