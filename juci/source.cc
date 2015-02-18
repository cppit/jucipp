#include "source.h"
#include <iostream>

//////////////
//// View ////
//////////////
Source::View::View() {
  std::cout << "View constructor run" << std::endl;
}
// Source::View::UpdateLine
// returns the new line
string Source::View::UpdateLine() {
  Gtk::TextIter line(get_buffer()->get_insert()->get_iter());
  // for each word --> check what it is --> apply appropriate tag
  
  //  retUrn "";
}

String Source::View::Getline(const Gtk::TextIter &begin) {
  Gtk::TextIter end(begin);
  while (!end.ends_line())
    end++;
  return begin.get_text(end);
}

// Source::View::ApplyTheme()
// Applies theme in textview
void Source::View::ApplyTheme(const Source::Theme &theme) {
  for (auto &item : theme.tagtable()) {
    get_buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

// Source::View::Theme::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Theme::tagtable() const {
  return tagtable_;
}

// Source::View::Theme::SetTagTable()
// sets the tagtable for the view
void Source::Theme::SetTagTable(
                        const std::unordered_map<string, string> &tagtable) {
  tagtable_ = tagtable;
}

///////////////
//// Model ////
///////////////
Source::Model::Model() {
  std::cout << "Model constructor run" << std::endl;
}

const string Source::Model::filepath() {
  return filepath_;
}

////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller() {
  std::cout << "Controller constructor run" << std::endl;
  view().get_buffer()->signal_changed().connect([this](){
      this->OnLineEdit();
    });
}
// Source::Controller::view()
// return shared_ptr to the view
Source::View& Source::Controller::view() {
  return view_;
}
// Source::Controller::model()
// return shared_ptr to the model()
Source::Model& Source::Controller::model() {
  return model_;
}
// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() {
  view().UpdateLine();
}
