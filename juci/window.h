#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include <iostream>
#include "gtkmm.h"
#include "api.h"
#include <cstddef>

class Window : public Gtk::Window {
public:
  Window();
  Gtk::Box window_box_;
//private:
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Notebook::Controller notebook_;

  //signal handlers
  void OnWindowHide();
  void OnOpenFile();
};

#endif  // JUCI_WINDOW_H_

