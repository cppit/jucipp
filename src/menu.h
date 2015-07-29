#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <string>
#include <unordered_map>
#include <gtkmm.h>
#include "logging.h"

class Menu {
public:
  Menu();
  Gtk::Widget& get_widget();
  Gtk::Menu& get_source_menu();
  void build();
  
  Gtk::Box box;
  std::unordered_map<std::string, std::string> key_map;
  std::string ui;
  Glib::RefPtr<Gtk::UIManager> ui_manager;
  Glib::RefPtr<Gtk::ActionGroup> action_group;
};
#endif  // JUCI_MENU_H_
