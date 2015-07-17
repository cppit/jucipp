#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "entry.h"
#include "source.h"
#include "directories.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>
#include "clangmm.h"

namespace Notebook {
  class View : public Gtk::Paned {
  public:
    View();
    Gtk::Notebook notebook;
  };
  class Controller {
  public:
    Controller();
    Source::View* CurrentSourceView();
    int CurrentPage();
    void OnCloseCurrentPage();
    void OnFileNewFile();
    bool OnSaveFile();
    bool OnSaveFile(std::string path);
    void OnDirectoryNavigation(const Gtk::TreeModel::Path& path,
                               Gtk::TreeViewColumn* column);
    void OnOpenFile(std::string filename);
    int Pages();
    void search(bool forward);
    View view;
    std::string OnSaveFileAs();
    std::string project_path;
    Directories::Controller directories; //Todo: make private after creating open_directory()
    Entry entry;
    std::vector<std::unique_ptr<Source> > source_views;
  private:
    void CreateKeybindings();
    void AskToSaveDialog();

    std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
    std::vector<std::unique_ptr<Gtk::HBox> > hboxes;
    Glib::RefPtr<Gtk::Clipboard> clipboard;
  };  // class controller
}  // namespace Notebook
#endif  // JUCI_NOTEBOOK_H_
