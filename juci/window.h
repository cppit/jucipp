#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "api.h"
#include "config.h"
#include <cstddef>


class Window : public Gtk::Window {
public:
  Window();
  Gtk::Box window_box_;
  MainConfig main_config;
  PluginApi api;
protected:
  bool on_key_press_event(GdkEventKey *event);
  bool on_delete_event (GdkEventAny *event);
private:
  std::mutex running;
  Gtk::VPaned paned_;
  //signal handlers
  void hide();
  void OnOpenFile();
  void OnFileOpenFolder();
  bool SaveFile();
  bool SaveFileAs();
};

#endif  // JUCI_WINDOW_H
