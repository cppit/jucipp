#ifndef JUCI_ENTRYBOX_H_
#define JUCI_ENTRYBOX_H_

#include <list>
#include <functional>
#include "gtkmm.h"

class EntryBox : public Gtk::Box {
public:
  class Entry : public Gtk::Entry {
  public:
    Entry(const std::string& content="", std::function<void(const std::string& content)> on_activate=nullptr, unsigned length=50);
    std::function<void(const std::string& content)> on_activate;
  };
  class Button : public Gtk::Button {
  public:
    Button(const std::string& label, std::function<void()> on_activate=nullptr);
    std::function<void()> on_activate;
  };
public:
  EntryBox();
  void clear();
  void show();
  std::list<Entry> entries;
  std::list<Button> buttons;
};

/*class Entry: public Gtk::Box {
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
};*/

#endif  // JUCI_ENTRYBOX_H_
