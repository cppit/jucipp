#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include <iostream>
#include "gtkmm.h"
#include "api.h"
#include "config.h"
#include <cstddef>


class Window : public Gtk::Window {
public:
  Window();
  MainConfig& main_config() {return main_config_;}
  Gtk::Box window_box_;
//private:
  MainConfig main_config_;
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Notebook::Controller notebook_;

  
 private:
  //signal handlers
  void OnWindowHide();
  void OnOpenFile();

};

#endif  // JUCI_WINDOW_H
