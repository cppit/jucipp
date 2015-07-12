#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "api.h"
#include "config.h"
#include "terminal.h"
#include <cstddef>


class Window : public Gtk::Window {
public:
  Window();
  // std::string  OnSaveFileAs();
  Gtk::Box window_box_;
  virtual ~Window() { }

  MainConfig main_config;
  Keybindings::Controller keybindings;
  Menu::Controller menu;
  Notebook::Controller notebook;
  Terminal::Controller terminal;
  PluginApi api;

 private:
  std::mutex running;
  Gtk::VPaned paned_;
  //signal handlers
  void OnWindowHide();
  void OnOpenFile();
  void OnFileOpenFolder();
  bool SaveFile();
  bool SaveFileAs();
};

#endif  // JUCI_WINDOW_H
