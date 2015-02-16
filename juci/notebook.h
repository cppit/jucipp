#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "keybindings.h"
#include "source.h"

namespace Notebook {
    class View : public Gtk::Box {
    public:
      View();
      Gtk::Box& view();
      Gtk::Notebook& notebook(){return notebook_;}
    protected:
      Gtk::Box view_;
      Gtk::Notebook notebook_;
    };
    class Controller {
    public:
      Controller(Keybindings::Controller keybindings);
      Gtk::Box& view();
      void OnNewPage(std::string name);
      void OnCloseCurrentPage();
    private:
      View view_;
      std::vector<Source::Controller*> source_vec_;
      std::vector<Gtk::ScrolledWindow*> scrolledwindow_vec_;
      void OnFileNewEmptyfile();
      void OnFileNewCCFile();
      void OnFileNewHeaderFile();
    };// class controller
} // namespace notebook


#endif  // JUCI_NOTEBOOK_H_
