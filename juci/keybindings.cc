#include "keybindings.h"

Keybindings::Model::Model(Keybindings::Config &config) :
  menu_ui_string_(config.menu_xml()) {
  /*  hidden_ui_string_ =
    "<ui>                                                   "
    "   <menubar name='MenuBar'>                            "
    "               <menuitem action='Test'/>               "
    "   </menubar>                                          "
    "</ui>                                                  ";*/
}

Keybindings::Model::~Model() {
}

Keybindings::Controller::Controller(Keybindings::Config &config) :
  config_(config), model_(config) {
  action_group_menu_ = Gtk::ActionGroup::create();
  ui_manager_menu_ = Gtk::UIManager::create();
  action_group_hidden_ = Gtk::ActionGroup::create();
  ui_manager_hidden_ = Gtk::UIManager::create();
}

Keybindings::Controller::~Controller() {
}

void Keybindings::Controller::BuildMenu() {
  try {
    ui_manager_menu_->add_ui_from_string(model_.menu_ui_string());
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed" << ex.what();
  }
  ui_manager_menu_->insert_action_group(action_group_menu_);
}
void Keybindings::Controller::BuildHiddenMenu() {
  try {
    ui_manager_hidden_->add_ui_from_string(model_.hidden_ui_string());
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building hidden menu failed" << ex.what();
  }
  ui_manager_hidden_->insert_action_group(action_group_hidden_);
}

Keybindings::Config::Config(Keybindings::Config &original) {
    SetMenu(original.menu_xml());
    SetKeyMap(original.key_map());
}

Keybindings::Config::Config() {
  menu_xml_ = "";
  std::unordered_map<std::string, std::string> key_map();
}

void Keybindings::Config::AppendXml(std::string &child) {
  menu_xml_ += child;
}

void Keybindings::Config::SetMenu(std::string &menu_xml) {
  menu_xml_ = menu_xml;
}

void Keybindings::Config::SetKeyMap(std::unordered_map<std::string, std::string> &key_map) {
  key_map_ = key_map;
}
