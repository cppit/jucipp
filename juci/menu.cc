#include "menu.h"
#include "singletons.h"

Menu::View::View(Gtk::Orientation orientation) :
  view_(orientation) {
  Gtk::MenuBar menutest;
  view_.pack_end(menutest);
}
Gtk::Box &Menu::View::view(
                           Glib::RefPtr<Gtk::UIManager> ui_manager) {
  view_.pack_start(*ui_manager->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
  return view_;
}

Menu::Controller::Controller() : menu_view_(Gtk::ORIENTATION_VERTICAL) {
  auto keybindings=Singleton::keybindings();
  keybindings->action_group_menu->add(Gtk::Action::create("FileNew",
                                                            "New File"));
  keybindings->action_group_menu->add(Gtk::Action::create("EditMenu",
                                                            Gtk::Stock::EDIT));
  keybindings->action_group_menu->add(Gtk::Action::create("WindowMenu",
                                                            "_Window"));
  keybindings->action_group_menu->add(Gtk::Action::create("WindowSplitWindow",
                                                            "Split window"),
                                        Gtk::AccelKey(Singleton::Config::keybindings()
						      ->key_map["split_window"]),//"<control><alt>S"),
                                        [this]() {
                                          OnWindowSplitWindow();
                                        });
  keybindings->action_group_menu->add(Gtk::Action::create("ProjectMenu",
                                                            "P_roject"));
  keybindings->action_group_menu->add(Gtk::Action::create("PluginMenu",
                                                            "_Plugins"));
  keybindings->action_group_menu->add(Gtk::Action::create("HelpMenu",
                                                            Gtk::Stock::HELP));
  keybindings->action_group_menu->add(Gtk::Action::create("HelpAbout",
                                                            Gtk::Stock::ABOUT),
                                        [this]() {
                                          OnHelpAbout();
                                        });
  keybindings->action_group_hidden->add(Gtk::Action::create("Test"),
                                          Gtk::AccelKey("<control><alt>K"),
                                          [this]() {
                                            OnHelpAbout();
                                          });
  //keybindings->BuildMenu(); // moved to window.cc
  keybindings->BuildHiddenMenu();
  }  // Controller
Gtk::Box &Menu::Controller::view() {
  return menu_view_.view(Singleton::keybindings()->ui_manager_menu);
}
void Menu::Controller::OnPluginAddSnippet() {
  //TODO(Forgi add you snippet magic code)
  std::cout << "Add snippet" << std::endl;
  //juci_api::py::LoadPlugin("snippet");
}
void Menu::Controller::OnFileOpenFile() {
  std::cout << "Open file clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::OnEditCut() {
  std::cout << "Clicked cut" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::OnEditFind() {
  std::cout << "Clicked find" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::OnWindowSplitWindow() {
  std::cout << "Clicked split window" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::OnHelpAbout() {
  std::cout << "Clicked about" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
