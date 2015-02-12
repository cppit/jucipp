#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include <iostream>
#include "gtkmm.h"
#include "menu.h"
#include "source.h"

class Window : public Gtk::Window {
public:
  Window();
  virtual ~Window() {}

private:
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Source::Controller& source();
  Source::Controller source_;
  Gtk::Box window_box_;

  //signal handlers
  void OnSystemQuit();
};

#endif  // JUCI_JUCI_H_
