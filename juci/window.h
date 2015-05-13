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
  // std::string  OnSaveFileAs();
  Gtk::Box window_box_;
  virtual ~Window() { }

  
//private:
  MainConfig main_config_;
  Keybindings::Controller keybindings_;
  Menu::Controller menu_;
  Notebook::Controller notebook_;
  Terminal::Controller terminal_;
  
  Keybindings::Controller& keybindings() { return keybindings_; }
 private:
  std::mutex running;
  Gtk::VPaned paned_;
  //signal handlers
  void OnWindowHide();
  void OnOpenFile();
  void OnFileOpenFolder();
  bool OnMouseRelease(GdkEventButton* button);  
};

#endif  // JUCI_WINDOW_H
