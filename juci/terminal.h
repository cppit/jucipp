#ifndef JUCI_TERMINAL_H_
#define JUCI_TERMINAL_H_

#include "gtkmm.h"

namespace Terminal {

  class View {
  public:
    View();   
    Gtk::HBox& view() {return view_;}
    Gtk::TextView& textview() {return textview_;}
  private:
    Gtk::HBox view_;
    Gtk::TextView textview_;
    Gtk::ScrolledWindow scrolledwindow_;
  };  // class view
  
  class Controller {  
  public:
    Controller();
    Gtk::HBox& view() {return view_.view();}
    Gtk::TextView& Terminal(){return view_.textview();}
  private:
    void ExecuteCommand();
    std::string getCommand();
    bool OnButtonRealeaseEvenet(GdkEventKey* key);
    Terminal::View view_;
    std::string root;


  };  // class controller
}  // namespace Terminal

#endif  // JUCI_TERMINAL_H_
