#include "view.h"


/***********************************/
/*              MENU               */
/***********************************/
View::Menu::Menu(Gtk::Orientation orientation) :
        view(orientation) {

    action_group = Gtk::ActionGroup::create();
    ui_manager = Gtk::UIManager::create();


}

void View::Menu::set_ui_manger_string(std::string ui_string) {
    try {
        ui_manager->add_ui_from_string(ui_string);
    }
    catch (const Glib::Error &ex) {
        std::cerr << "building menus failed: " << ex.what();
    }
}

void View::Menu::set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group) {
    ui_manager->insert_action_group(action_group);
}

Gtk::Box &View::Menu::get_view() {
    view.pack_start(*ui_manager->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
    return view;
}


View::Menu::~Menu() {
}