#include "source.h"
#include <iostream>

//////////////
//// View ////
//////////////
Source::View::View() {
  std::cout << "View constructor run" << std::endl;
}

void Source::View::UpdateLine(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  std::cout << "Update line called. linum: ";
  std::cout << mark->get_iter().get_line() << std::endl;
}

///////////////
//// Model ////
///////////////
Source::Model::Model() {
  std::cout << "Model constructor run" << std::endl;
}

////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller() {
  std::cout << "Controller constructor run" << std::endl;
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
void Source::Controller::OnLineEdit(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {}
