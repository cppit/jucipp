/*juCi++ main header file*/
#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include <iostream>
#include "gtkmm.h"
#include "menu.h"
#include "source.h"

class Window : public Gtk::Window {
public:
  Window();

  virtual ~Window() {
  }

  std::shared_ptr<Source::Controller> source();

private:
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Source::Controller source_;
  Gtk::Box window_box_;

  /*signal handler*/
  void OnSystemQuit();
};

#endif  // JUCI_JUCI_H_
