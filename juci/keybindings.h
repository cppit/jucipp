//juCi++ class that holds every keybinding.
#ifndef JUCI_KEYBINDINGS_H_
#define JUCI_KEYBINDINGS_H_

#include <gtkmm.h>
#include <iostream>
#include <unordered_map>

class Keybindings {
public:
  class Config {
  public:
    void AppendXml(std::string &child);
    void SetMenu(std::string &menu_xml);
    void SetKeyMap(std::unordered_map<std::string, std::string> &key_map);
    std::unordered_map<std::string, std::string> key_map;
    std::string menu_xml;
  };//Config
  
  Keybindings();
  void BuildMenu();
  void BuildHiddenMenu();
  
  std::string menu_ui_string;
  std::string hidden_ui_string;
  
  Glib::RefPtr<Gtk::UIManager> ui_manager_menu;
  Glib::RefPtr<Gtk::ActionGroup> action_group_menu;
  Glib::RefPtr<Gtk::UIManager> ui_manager_hidden;
  Glib::RefPtr<Gtk::ActionGroup> action_group_hidden;
};
#endif  // JUCI_KEYBINDINGS_H_
