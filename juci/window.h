#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "api.h"
#include "config.h"
#include "terminal.h"
#include <cstddef>


class Window : public Gtk::Window {
public:
  Window();
  MainConfig& main_config() { return main_config_; }
  Gtk::Box window_box_;


  
//private:
  MainConfig main_config_;
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Notebook::Controller notebook_;
  Terminal::Controller terminal_;
  
  Keybindings::Controller& keybindings() { return keybindings_; }
 private:
  //signal handlers
  void OnWindowHide();
  void OnOpenFile();
  void OnFileOpenFolder();
  void OnSaveFileAs();
  bool OnKeyRelease(GdkEventKey* key);
  bool OnMouseRelease(GdkEventButton* button);
  
};

#endif  // JUCI_WINDOW_H
