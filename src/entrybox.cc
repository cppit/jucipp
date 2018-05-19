#include "entrybox.h"

std::unordered_map<std::string, std::vector<std::string> > EntryBox::entry_histories;

EntryBox::Entry::Entry(const std::string& content, std::function<void(const std::string& content)> on_activate_, unsigned width_chars) : Gtk::Entry(), on_activate(std::move(on_activate_)) {
  set_max_length(0);
  set_width_chars(width_chars);
  set_text(content);
  selected_history=0;
  signal_activate().connect([this](){
    if(this->on_activate) {
      auto &history=EntryBox::entry_histories[get_placeholder_text()];
      auto text=get_text();
      if(history.size()==0 || (history.size()>0 && *history.begin()!=text))
        history.emplace(history.begin(), text);
      selected_history=0;
      this->on_activate(text);
    }
  });
  signal_key_press_event().connect([this](GdkEventKey* key){
    if(key->keyval==GDK_KEY_Up || key->keyval==GDK_KEY_KP_Up) {
      auto &history=entry_histories[get_placeholder_text()];
      if(history.size()>0) {
        selected_history++;
        if(selected_history>=history.size())
          selected_history=history.size()-1;
        set_text(history[selected_history]);
        set_position(-1);
      }
    }
    if(key->keyval==GDK_KEY_Down || key->keyval==GDK_KEY_KP_Down) {
      auto &history=entry_histories[get_placeholder_text()];
      if(history.size()>0) {
        if(selected_history!=0)
          selected_history--;
        set_text(history[selected_history]);
        set_position(-1);
      }
    }
    return false;
  });
}

EntryBox::Button::Button(const std::string& label, std::function<void()> on_activate_) : Gtk::Button(label), on_activate(std::move(on_activate_)) {
  set_focus_on_click(false);
  signal_clicked().connect([this](){
    if(this->on_activate)
      this->on_activate();
  });
}

EntryBox::ToggleButton::ToggleButton(const std::string& label, std::function<void()> on_activate_) : Gtk::ToggleButton(label), on_activate(std::move(on_activate_)) {
  set_focus_on_click(false);
  signal_clicked().connect([this](){
    if(this->on_activate)
      this->on_activate();
  });
}

EntryBox::Label::Label(std::function<void(int state, const std::string& message)> update_) : Gtk::Label(), update(std::move(update_)) {
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
