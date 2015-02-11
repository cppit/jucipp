#include "source.h"
#include <iostream>

using namespace std;


//////////////
//// View ////
//////////////
Source::View::View() {
  cout << "View construktor run" << endl;
}

void Source::View::UpdateLine(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  cout << "Update line called. linum: " << mark->get_iter().get_line() << endl;
}

///////////////
//// Model ////
///////////////
Source::Model::Model(){
  cout << "Model construktor run" << endl;
}

////////////////////
//// Controller ////
////////////////////

/**
 *
 */
Source::Controller::Controller() {
  cout << "Controller construktor run" << endl;
}
/**
 *
 */
std::shared_ptr<Source::View> Source::Controller::sourceview() {
  sourceview();
  return std::shared_ptr<Source::View>(&sourceview_);
}
/**
 *
 */
void Source::Controller::OnLineEdit(Glib::RefPtr<Gtk::TextBuffer::Mark> mark){

}
