#include "keybindings.h"
#include "singletons.h"

Keybindings::Keybindings() {
  menu_ui_string=Singleton::Config::keybindings()->menu_xml;
  action_group_menu = Gtk::ActionGroup::create();
  ui_manager_menu = Gtk::UIManager::create();
  action_group_hidden = Gtk::ActionGroup::create();
  ui_manager_hidden = Gtk::UIManager::create();
}

void Keybindings::BuildMenu() {
  try {
    ui_manager_menu->add_ui_from_string(menu_ui_string);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed" << ex.what();
  }
  ui_manager_menu->insert_action_group(action_group_menu);
}
void Keybindings::BuildHiddenMenu() {
  try {
    ui_manager_hidden->add_ui_from_string(hidden_ui_string);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building hidden menu failed" << ex.what();
  }
  ui_manager_hidden->insert_action_group(action_group_hidden);
}

void Keybindings::Config::AppendXml(std::string &child) {
  menu_xml += child;
}

void Keybindings::Config::SetMenu(std::string &menu_xml) {
  menu_xml = menu_xml;
}

void Keybindings::Config::SetKeyMap(std::unordered_map<std::string, std::string> &key_map) {
  key_map = key_map;
}
