#include "juci.h"

Window::Window() :
        window_box_(Gtk::ORIENTATION_HORIZONTAL),
        menu() {


    set_title("example juCi++");
    set_default_size(600, 600);

    add(window_box_);

    add_accel_group(menu.ui_manager()->get_accel_group());

    window_box_.pack_start(menu.view());


    show_all_children();
};


Window::~Window() {

}


