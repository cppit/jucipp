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

public:
  EntryBox();
  Gtk::Box upper_box;
  Gtk::Box lower_box;
  void clear();
  void hide() {clear();}
  void show();
  std::list<Entry> entries;
  std::list<Button> buttons;
  std::list<ToggleButton> toggle_buttons;
  std::list<Label> labels;
};

#endif  // JUCI_ENTRYBOX_H_
