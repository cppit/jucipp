#include "keybindings.h"


 Keybindings::Controller::Controller() {
   action_group_ = Gtk::ActionGroup::create();
   ui_manager_ = Gtk::UIManager::create();
 }
Keybindings::Controller::~Controller(){

}
void Keybindings::Controller::set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group) {
  ui_manager_->insert_action_group(action_group);
}
void Keybindings::Controller::set_ui_manger_string(std::string ui_string) {
  try {
    ui_manager_->add_ui_from_string(ui_string);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menus failed: " << ex.what();
  }
}
