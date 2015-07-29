#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "entrybox.h"
#include "source.h"
#include "directories.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>
#include "clangmm.h"

class Notebook : public Gtk::Notebook {
public:
  Notebook();
  Source::View* CurrentSourceView();
  int CurrentPage();
  bool close_current_page();
  void open_file(std::string filename);
  int Pages();
  std::string project_path;
      
  std::vector<std::unique_ptr<Source> > source_views;
private:
  bool save_modified_dialog();

  std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
  std::vector<std::unique_ptr<Gtk::HBox> > hboxes;
};
#endif  // JUCI_NOTEBOOK_H_
