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
  set_focus_on_click(false);
  signal_clicked().connect([this](){
    if(this->on_activate)
      this->on_activate();
  });
}

EntryBox::ToggleButton::ToggleButton(const std::string& label, std::function<void()> on_activate) : Gtk::ToggleButton(label), on_activate(on_activate) {
  set_focus_on_click(false);
  signal_clicked().connect([this](){
    if(this->on_activate)
      this->on_activate();
  });
}

EntryBox::Label::Label(std::function<void(int state, const std::string& message)> update) : Gtk::Label(), update(update) {
    if(this->update)
      this->update(-1, "");
}

EntryBox::EntryBox() : Gtk::Box(Gtk::ORIENTATION_VERTICAL), upper_box(Gtk::ORIENTATION_HORIZONTAL), lower_box(Gtk::ORIENTATION_HORIZONTAL) {
  pack_start(upper_box, Gtk::PACK_SHRINK);
  pack_start(lower_box, Gtk::PACK_SHRINK);
  this->set_focus_chain({&lower_box});
}

void EntryBox::clear() {
  Gtk::Box::hide();
  entries.clear();
  buttons.clear();
  toggle_buttons.clear();
  labels.clear();
}

void EntryBox::show() {
  std::vector<Gtk::Widget*> focus_chain;
  for(auto& entry: entries) {
    lower_box.pack_start(entry, Gtk::PACK_SHRINK);
    focus_chain.emplace_back(&entry);
  }
  for(auto& button: buttons)
    lower_box.pack_start(button, Gtk::PACK_SHRINK);
  for(auto& toggle_button: toggle_buttons)
    lower_box.pack_start(toggle_button, Gtk::PACK_SHRINK);
  for(auto& label: labels)
    upper_box.pack_start(label, Gtk::PACK_SHRINK);
  lower_box.set_focus_chain(focus_chain);
  show_all();
  if(entries.size()>0) {
    entries.begin()->grab_focus();
    entries.begin()->select_region(0, entries.begin()->get_text_length());
  }
}
