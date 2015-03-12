#ifndef JUCI_ENTRY_H_
#define JUCI_ENTRY_H_

#include <iostream>
#include "gtkmm.h"
#include "keybindings.h"

namespace Entry {
  class View  {
  public:
    View();
    Gtk::Box& view();
    Gtk::Entry& entry(){return entry_;}
    Gtk::Button& button_apply(){return button_apply_;};
    Gtk::Button& button_close(){return button_close_;};
    Gtk::Button& button_next(){return button_next_;};
    Gtk::Button& button_prev(){return button_prev_;};
    void OnShowSetFilenName(std::string exstension);
    void OnShowSearch(std::string current);
    void OnHideEntry(bool is_new_file);
  protected:
    Gtk::Box view_;
    Gtk::Entry entry_;
    Gtk::Button button_apply_, button_close_, button_next_, button_prev_;
  };
  class Controller {
  public:
    Controller();
    Gtk::Box& view();
    Gtk::Button& button_apply();
    Gtk::Button& button_close();
    Gtk::Button& button_next();
    Gtk::Button& button_prev();
    
    std::string text();
    void OnShowSetFilenName(std::string exstension);
    void OnShowSearch(std::string current);
    void OnHideEntries(bool is_new_file);
    View view_;
  };// class controller
} // namespace notebook


#endif  // JUCI_ENTRY_H_
