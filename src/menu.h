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
  void init(); //For Ubuntu 14...
  Gtk::Application* application; //For Ubuntu 14...
  
  void add_action(const std::string &name, std::function<void()> action);
  std::unordered_map<std::string, Glib::RefPtr<Gio::SimpleAction> > actions;
  void set_keys();
  
  void build();
  Glib::RefPtr<Gtk::Builder> builder;
  
  std::string ui_xml;
};
#endif  // JUCI_MENU_H_
