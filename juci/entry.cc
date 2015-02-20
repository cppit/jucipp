#include "entry.h"

Entry::Model::Model() :
  next_("Next"),
  prev_("Prev"){
  std::cout<<"Model Entry"<<std::endl;
}
Entry::View::View() :
  view_(Gtk::ORIENTATION_HORIZONTAL),
  button_apply_(Gtk::Stock::APPLY),
  button_cancel_(Gtk::Stock::CANCEL){
  }
Gtk::Box& Entry::View::view() {
  return view_;
}
void Entry::View::OnShowSetFilenName(std::string exstension) {
  entry_.set_max_length(50);
  entry_.set_text(exstension);
  view_.pack_start(entry_);
  view_.pack_end(button_cancel_, Gtk::PACK_SHRINK);
  view_.pack_end(button_apply_, Gtk::PACK_SHRINK); 
}
void Entry::View::OnHideEntry()
{
  view_.remove(entry_);
  view_.remove(button_cancel_);
  view_.remove(button_apply_);
}
Entry::Controller::Controller() {					
}
Gtk::Box& Entry::Controller::view() {
  return view_.view();
}
Gtk::Button& Entry::Controller::button_apply(){
  return view_.button_apply();
}
void Entry::Controller::OnShowSetFilenName(std::string exstension) {
  view_.OnShowSetFilenName(exstension);
  view_.view().show_all();
  view_.entry().grab_focus();
  view_.entry().set_position(0);
}
void Entry::Controller::OnHideEntries(){
  view_.OnHideEntry();
}
