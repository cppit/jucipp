#include "entry.h"

Entry::Entry() :
  Gtk::Box(Gtk::ORIENTATION_HORIZONTAL),
  button_apply_set_filename(Gtk::Stock::APPLY),
  button_close(Gtk::Stock::CLOSE),
  button_next("Next"),
  button_prev("Prev"){
  entry.signal_activate().connect([this](){
    if(activate) {
      activate();
    }
  });
  entry.signal_key_press_event().connect(sigc::mem_fun(*this, &Entry::on_key_press), false);
}

bool Entry::on_key_press(GdkEventKey* key) {
  if(key->keyval==GDK_KEY_Escape)
    hide();
  return false;
}

void Entry::show_set_filename() {
  hide();
  entry.set_max_length(50);
  entry.set_text("");
  pack_start(entry);
  pack_end(button_close, Gtk::PACK_SHRINK);
  pack_end(button_apply_set_filename, Gtk::PACK_SHRINK);
  show_all();
  entry.grab_focus();
  entry.set_position(0);
  activate=[this](){
    button_apply_set_filename.clicked();
  };
}

void Entry::show_search(const std::string& current){
  hide();
  entry.set_max_length(50);
  entry.set_text(current);
  pack_start(entry);
  pack_start(button_next, Gtk::PACK_SHRINK);
  pack_start(button_prev, Gtk::PACK_SHRINK);
  pack_end(button_close, Gtk::PACK_SHRINK);
  show_all();
  entry.grab_focus();
  entry.set_position(0);
  activate=[this](){
    button_next.clicked();
  };
}
void Entry::hide() {
  auto widgets=get_children();
  for(auto &w: widgets)
    remove(*w);
}

std::string Entry::operator()() {
  return entry.get_text();
}
