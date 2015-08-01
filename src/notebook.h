#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "source.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>

class Notebook : public Gtk::Notebook {
public:
  Notebook();
  Source::View* get_view(int page);
  int size();
  Source::View* get_current_view();
  bool close_current_page();
  void open(std::string filename);
  bool save(int page);
  bool save_current();
  std::string project_path;
      
private:
  bool save_modified_dialog();
  std::vector<Source::View*> source_views; //Is NOT freed in destructor, this is intended for quick program exit.
  std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
  std::vector<std::unique_ptr<Gtk::HBox> > hboxes;
};
#endif  // JUCI_NOTEBOOK_H_
