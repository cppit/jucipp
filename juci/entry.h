#ifndef JUCI_ENTRY_H_
#define JUCI_ENTRY_H_

#include <iostream>
#include <functional>
#include "gtkmm.h"

class Entry: public Gtk::Box {
public:
  Entry();
  void show_set_filename();
  void show_search(const std::string& current);
  void hide();
  std::string operator()();
  Gtk::Entry entry;
  Gtk::Button button_apply_set_filename, button_close, button_next, button_prev;
private:
  bool on_key_press(GdkEventKey* key);
  std::function<void()> activate;
};

#endif  // JUCI_ENTRY_H_
