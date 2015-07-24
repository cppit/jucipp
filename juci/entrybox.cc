#include "entrybox.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

EntryBox::Entry::Entry(const std::string& content, std::function<void(const std::string& content)> on_activate, unsigned length) : Gtk::Entry(), on_activate(on_activate) {
  set_max_length(length);
  set_text(content);
  signal_activate().connect([this](){
    if(this->on_activate)
      this->on_activate(get_text());
  });
}

EntryBox::Button::Button(const std::string& label, std::function<void()> on_activate) : Gtk::Button(label), on_activate(on_activate) {
  signal_clicked().connect([this](){
    if(this->on_activate)
      this->on_activate();
  });
}

EntryBox::EntryBox() : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {}

void EntryBox::clear() {
  hide();
  entries.clear();
  buttons.clear();
}

void EntryBox::show() {
  std::vector<Gtk::Widget*> focus_chain;
  for(auto& entry: entries) {
    pack_start(entry, Gtk::PACK_SHRINK);
    focus_chain.emplace_back(&entry);
  }
  for(auto& button: buttons)
    pack_start(button, Gtk::PACK_SHRINK);
    
  set_focus_chain(focus_chain);
  show_all();
  if(entries.size()>0) {
    entries.begin()->grab_focus();
    entries.begin()->select_region(0, entries.begin()->get_text_length());
  }
}
