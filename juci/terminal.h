 
#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"

namespace Terminal {

  class View {
  public:
    View();   
    //Gtk::HBox view() {return view_;}
  private:
    Gtk::HBox view_;
    Gtk::TextBuffer buffer_;
    Gtk::TextView textview_;
  };  // class view
  
  class Controller {
  public:


  };  // class controller
}  // namespace Terminal

#endif  // JUCI_NOTEBOOK_H_
