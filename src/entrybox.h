#ifndef JUCI_ENTRYBOX_H_
#define JUCI_ENTRYBOX_H_

#include <list>
#include <functional>
#include "gtkmm.h"
#include <unordered_map>
#include <string>
#include <vector>

class EntryBox : public Gtk::Box {
public:
  class Entry : public Gtk::Entry {
  public:
    Entry(const std::string& content="", std::function<void(const std::string& content)> on_activate=nullptr, unsigned width_chars=-1);
    std::function<void(const std::string& content)> on_activate;
  private:
    size_t selected_history;
  };
  class Button : public Gtk::Button {
  public:
    Button(const std::string& label, std::function<void()> on_activate=nullptr);
    std::function<void()> on_activate;
  };
  class ToggleButton : public Gtk::ToggleButton {
  public:
    ToggleButton(const std::string& label, std::function<void()> on_activate=nullptr);
    std::function<void()> on_activate;
  };
  class Label : public Gtk::Label {
  public:
    Label(std::function<void(int state, const std::string& message)> update=nullptr);
    std::function<void(int state, const std::string& message)> update;
  };
  
private:
  EntryBox();
public:
  static EntryBox &get() {
    static EntryBox singleton;
    return singleton;
  }
  
  Gtk::Box upper_box;
  Gtk::Box lower_box;
  void clear();
  void hide() {clear();}
  void show();
  std::list<Entry> entries;
  std::list<Button> buttons;
  std::list<ToggleButton> toggle_buttons;
  std::list<Label> labels;
  
private:
  static std::unordered_map<std::string, std::vector<std::string> > entry_histories;
};

#endif  // JUCI_ENTRYBOX_H_
