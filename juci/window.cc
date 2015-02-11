#include "juci.h"

Window::Window() :
  window_box_(Gtk::ORIENTATION_HORIZONTAL),
  menu_(keybindings_) {
  set_title("example juCi++");
  set_default_size(600, 600);
  //  window_box_.add(*source_.sourceview());
  add(window_box_);
  add_accel_group(keybindings_.ui_manager()->get_accel_group());
  window_box_.pack_start(menu_.view());
  show_all_children();
  
}

std::shared_ptr<Source::Controller> Window::source() {
  return std::shared_ptr<Source::Controller>(&source_);
}
