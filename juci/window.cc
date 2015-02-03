#include "juci.h"

Window::Window() :
        window_box(Gtk::ORIENTATION_HORIZONTAL),
        menu() {


    set_title("example juCi++");
    set_default_size(600, 600);

    add(window_box);
    menu.get_action_group()->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
            [this]() {
                onSystemQuit();
            });

    add_accel_group(menu.get_ui_manager()->get_accel_group());

    window_box.pack_start(menu.get_view());


    show_all_children();
};

void Window::onSystemQuit() {
    hide();
}

Window::~Window() {

}


