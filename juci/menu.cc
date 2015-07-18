#include "menu.h"
#include <iostream>

Menu::Menu() : box(Gtk::ORIENTATION_VERTICAL) {
  action_group = Gtk::ActionGroup::create();
  ui_manager = Gtk::UIManager::create();

  action_group->add(Gtk::Action::create("FileNew", "New File"));
  action_group->add(Gtk::Action::create("EditMenu", Gtk::Stock::EDIT));
  action_group->add(Gtk::Action::create("WindowMenu", "_Window"));
  action_group->add(Gtk::Action::create("WindowSplitWindow", "Split window"), Gtk::AccelKey(key_map["split_window"]), [this]() {
  });
  action_group->add(Gtk::Action::create("ProjectMenu", "P_roject"));
  action_group->add(Gtk::Action::create("PluginMenu", "_Plugins"));
  action_group->add(Gtk::Action::create("HelpMenu", Gtk::Stock::HELP));
  action_group->add(Gtk::Action::create("HelpAbout", Gtk::Stock::ABOUT), [this]() {
  });
}

Gtk::Widget& Menu::get_widget() {
  return *ui_manager->get_widget("/MenuBar");
}

Gtk::Menu& Menu::get_cpp() {
  return *(Gtk::Menu*)ui_manager->get_widget("/MenuBar/CppMenu");
}

void Menu::build() {
  try {
    ui_manager->add_ui_from_string(ui);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed" << ex.what();
  }
  ui_manager->insert_action_group(action_group);
}
