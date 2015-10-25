#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <string>
#include <unordered_map>
#include <gtkmm.h>

class Menu {
public:
  class Config {
  public:
    std::unordered_map<std::string, std::string> keys;
  };
  
  Menu();
  Gtk::Widget& get_widget();
  void build();
  
  std::string ui_xml;
  Glib::RefPtr<Gtk::Builder> builder;
};
#endif  // JUCI_MENU_H_
