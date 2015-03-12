#include "entry.h"
Entry::View::View() :
  view_(Gtk::ORIENTATION_HORIZONTAL),
  button_apply_(Gtk::Stock::APPLY),
  button_close_(Gtk::Stock::CLOSE),
  button_next_("Next"),
  button_prev_("Prev"){
}
Gtk::Box& Entry::View::view() {
  return view_;
}
void Entry::View::OnShowSetFilenName(std::string exstension) {
  entry_.set_max_length(50);
  entry_.set_text(exstension);
  view_.pack_start(entry_);
  view_.pack_end(button_close_, Gtk::PACK_SHRINK);
  view_.pack_end(button_apply_, Gtk::PACK_SHRINK); 
}
void Entry::View::OnShowSearch(std::string current){
  entry_.set_max_length(50);
  entry_.set_text(current);
  view_.pack_start(entry_);
  view_.pack_start(button_next_, Gtk::PACK_SHRINK);
  view_.pack_start(button_prev_, Gtk::PACK_SHRINK);
  view_.pack_end(button_close_, Gtk::PACK_SHRINK);
}
void Entry::View::OnHideEntry(bool is_new_file)
{
  view_.remove(entry_);
  view_.remove(button_close_);
  if(!is_new_file){
    view_.remove(button_next_);
    view_.remove(button_prev_);
  }else{
    view_.remove(button_apply_);
  }
}

Entry::Controller::Controller() {					
}
Gtk::Box& Entry::Controller::view() {
  return view_.view();
}
void Entry::Controller::OnShowSetFilenName(std::string exstension) {
  view_.OnShowSetFilenName(exstension);
  view_.view().show_all();
  view_.entry().grab_focus();
  view_.entry().set_position(0);
}
void Entry::Controller::OnShowSearch(std::string current){
  view_.OnShowSearch(current);
  view_.view().show_all();
  view_.entry().grab_focus();
  view_.entry().set_position(0);
}
void Entry::Controller::OnHideEntries(bool is_new_file){
  view_.OnHideEntry(is_new_file);
}
std::string Entry::Controller::text(){
  return view_.entry().get_text();
}
Gtk::Button& Entry::Controller::button_apply(){
  return view_.button_apply();
}
Gtk::Button& Entry::Controller::button_close(){
  return view_.button_close();
}
Gtk::Button& Entry::Controller::button_next(){
  return view_.button_next();
}
Gtk::Button& Entry::Controller::button_prev(){
  return view_.button_prev();
}

