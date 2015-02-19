#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "entry.h"
#include "source.h"

namespace Notebook {
  class View {
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
    Controller(Keybindings::Controller& keybindings);
    Gtk::Box& view();
    Gtk::Box& entry_view();
    void OnNewPage(std::string name);
    void OnCloseCurrentPage();
  private:
    View view_;
    Entry::Controller entry_;
    std::vector<Source::Controller*> source_vec_;
    std::vector<Gtk::ScrolledWindow*> scrolledwindow_vec_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard;
    std::list<Gtk::TargetEntry> listTargets;
    void OnFileNewEmptyfile();
    void OnFileNewCCFile();
    void OnFileNewHeaderFile();
    void OnEditCopy();
    void OnEditPaste();
    void OnEditCut();
    void OnOpenFile(std::string name, std::string content);
  };  // class controller
}  // namespace Notebook


#endif  // JUCI_NOTEBOOK_H_
