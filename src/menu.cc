#include "menu.h"
#include "logging.h"

Menu::Menu() : box(Gtk::ORIENTATION_VERTICAL) {
  INFO("Creating menu");
  action_group = Gtk::ActionGroup::create();
  ui_manager = Gtk::UIManager::create();

  action_group->add(Gtk::Action::create("FileMenu", "File"));
  action_group->add(Gtk::Action::create("EditMenu", "Edit"));
  action_group->add(Gtk::Action::create("WindowMenu", "_Window"));
  action_group->add(Gtk::Action::create("WindowSplitWindow", "Split window"), Gtk::AccelKey(key_map["split_window"]), [this]() {
  });
  action_group->add(Gtk::Action::create("ProjectMenu", "P_roject"));
  action_group->add(Gtk::Action::create("SourceMenu", "_Source"));
  action_group->add(Gtk::Action::create("PluginMenu", "_Plugins"));
  action_group->add(Gtk::Action::create("HelpMenu", "Help"));
  action_group->add(Gtk::Action::create("HelpAbout", "About"), [this]() {
  });
  INFO("Menu created");
}

Gtk::Widget& Menu::get_widget() {
  return *ui_manager->get_widget("/MenuBar");
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
