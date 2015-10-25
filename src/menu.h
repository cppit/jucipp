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
  void add_action(const std::string &name, std::function<void()> action);
  std::unordered_map<std::string, Glib::RefPtr<Gio::SimpleAction> > actions;
  
  void build();
  
  std::string ui_xml;
  Glib::RefPtr<Gtk::Builder> builder;
};
#endif  // JUCI_MENU_H_
