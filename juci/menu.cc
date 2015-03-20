#include "menu.h"

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

Menu::Controller::Controller(Keybindings::Controller& keybindings) :
  menu_view_(Gtk::ORIENTATION_VERTICAL),
  keybindings_(keybindings) {
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileNew",
                                                            Gtk::Stock::FILE));

  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFolder",
                                                            "Open folder"),
                                        [this]() {
                                          OnFileOpenFolder();
                                        });
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditMenu",
                                                            Gtk::Stock::EDIT));
  keybindings_.action_group_menu()->add(Gtk::Action::create("WindowMenu",
                                                            "_Window"));
  keybindings_.action_group_menu()->add(Gtk::Action::create("WindowSplitWindow",
                                                            "Split window"),
                                        Gtk::AccelKey(keybindings_.config_
						      .key_map()["split_window"]),//"<control><alt>S"),
                                        [this]() {
                                          OnWindowSplitWindow();
                                        });
  keybindings_.action_group_menu()->add(Gtk::Action::create("PluginMenu",
                                                            "_Plugins"));
  keybindings_.action_group_menu()->add(Gtk::Action::create("HelpMenu",
                                                            Gtk::Stock::HELP));
  keybindings_.action_group_menu()->add(Gtk::Action::create("HelpAbout",
                                                            Gtk::Stock::ABOUT),
                                        [this]() {
                                          OnHelpAbout();
                                        });
  keybindings_.action_group_hidden()->add(Gtk::Action::create("Test"),
                                          Gtk::AccelKey("<control><alt>K"),
                                          [this]() {
                                            OnHelpAbout();
                                          });
  //keybindings_.BuildMenu(); // moved to window.cc
  keybindings_.BuildHiddenMenu();
  }  // Controller
Gtk::Box &Menu::Controller::view() {
  return menu_view_.view(keybindings_.ui_manager_menu());
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
void Menu::Controller::OnFileOpenFolder() {
  std::cout << "Open folder clicked" << std::endl;
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
