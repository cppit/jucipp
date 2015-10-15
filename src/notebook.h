#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "source.h"
#include "source_clang.h"
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>
#include "directories.h"

class Notebook : public Gtk::Notebook {
public:
  Notebook(Directories &directories);
  Source::View* get_view(int page);
  size_t get_index(int page);
  int size();
  Source::View* get_current_view();
  bool close_current_page();
  void open(const boost::filesystem::path &file_path);
  bool save(int page, bool reparse_needed=false);
  bool save_current();
  void configure(int view_nr);
      
private:
  bool save_modified_dialog();
  Directories &directories;
  std::vector<Source::View*> source_views; //Is NOT freed in destructor, this is intended for quick program exit.
  std::vector<std::unique_ptr<Gtk::Widget> > source_maps;
  std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
  std::vector<std::unique_ptr<Gtk::HBox> > hboxes;
};
#endif  // JUCI_NOTEBOOK_H_
