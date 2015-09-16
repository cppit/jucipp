#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <string>
#include <unordered_map>
#include <gtkmm.h>

class Menu {
public:
  Menu();
  Gtk::Widget& get_widget();
  void build();
  
  std::unordered_map<std::string, std::string> key_map;
  std::string ui_xml;
  Glib::RefPtr<Gtk::UIManager> ui_manager;
  Glib::RefPtr<Gtk::ActionGroup> action_group;
};
#endif  // JUCI_MENU_H_
