#include "controller.h"

Controller::Menu::Menu() :
        menu_view(Gtk::ORIENTATION_VERTICAL),
        menu_model() {
/*Add action to menues*/
    menu_view.get_action_group()->add(Gtk::Action::create("FileMenu", "File"));

    menu_view.get_action_group()->add(Gtk::Action::create("FileNewStandard",
                    Gtk::Stock::NEW, "New empty file", "Create a new file"),
            [this]() {
                onNewEmptyfile();
            });

    menu_view.set_ui_manager_action_group(menu_view.get_action_group());
    menu_view.set_ui_manger_string(menu_model.get_ui_string());
}

Controller::Menu::~Menu() {

}

Gtk::Box &Controller::Menu::get_view() {
    return menu_view.get_view();
}

void Controller::Menu::onNewEmptyfile() {
    std::cout << "New file clicked"<< std::endl;
}




