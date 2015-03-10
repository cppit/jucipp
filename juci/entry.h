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
    Gtk::Button& button_cancel(){return button_cancel_;};
    void OnShowSetFilenName(std::string exstension);
    void OnShowSearch(std::string current);
    void OnHideEntry();
  protected:
    Gtk::Box view_;
    Gtk::Entry entry_;
    Gtk::Button button_apply_, button_cancel_, button_next_, button_prev_;
  };
  class Model {
  public:
    Model();
    std::string next(){return next_;};
    std::string prev(){return prev_;};
  private:
    std::string next_, prev_;    
  };
  class Controller {
  public:
    Controller();
    Gtk::Box& view();
    Gtk::Button& button_apply();
    std::string text();
    void OnShowSetFilenName(std::string exstension);
    void OnShowSearch(std::string current);
    void OnHideEntries();
    View view_;
  };// class controller
} // namespace notebook


#endif  // JUCI_ENTRY_H_
