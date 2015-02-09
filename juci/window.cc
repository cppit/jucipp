#include "juci.h"

Window::Window() :
        window_box_(Gtk::ORIENTATION_HORIZONTAL),
        keybindings(),
        menu(keybindings) {
  set_title("example juCi++");
  set_default_size(600, 600);
  add(window_box_);
  window_box_.pack_start(menu.view());
  add_accel_group(keybindings.ui_manager()->get_accel_group());
  show_all_children();
};

Window::~Window() {

}


