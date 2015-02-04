#include "view.h"


/***********************************/
/*              MENU               */
/***********************************/
View::Menu::Menu(Gtk::Orientation orientation) :
        view_(orientation) {

    action_group_ = Gtk::ActionGroup::create();
    ui_manager_ = Gtk::UIManager::create();


}

void View::Menu::set_ui_manger_string(std::string ui_string) {
    try {
        ui_manager_->add_ui_from_string(ui_string);
    }
    catch (const Glib::Error &ex) {
        std::cerr << "building menus failed: " << ex.what();
    }
}

void View::Menu::set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group) {
    ui_manager_->insert_action_group(action_group);
}

Gtk::Box &View::Menu::view() {
    view_.pack_start(*ui_manager_->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
    return view_;
}


View::Menu::~Menu() {
}