#ifndef JUCI_NOTEBOOK_H_
#define JUCI_NOTEBOOK_H_

#include <iostream>
#include "gtkmm.h"
#include "entry.h"
#include "source.h"

namespace Notebook {
  class Model {
  public:
    Model();
    std::string cc_extension;
    std::string h_extension;
  };
  class View {
  public:
    View();
    Gtk::Box& view();
    Gtk::Notebook& notebook() { return notebook_; }
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
    void OnOpenFile(std::string filename);
    View view_;
    Model model_;
    Entry::Controller entry_;
    std::vector<Source::Controller*> source_vec_;
    std::vector<Gtk::ScrolledWindow*> scrolledwindow_vec_;
    std::vector<Gtk::Label*> label_vec_;
    std::vector<Gtk::Paned*> paned_vec_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard;
    std::list<Gtk::TargetEntry> listTargets;
    std::string GetCursorWord();
    Glib::RefPtr<Gtk::TextBuffer> buffer();
    Gtk::Notebook& notebook();
    int currentPage();
    int pages();
    void OnFileNewEmptyfile();
    void OnFileNewCCFile();
    void OnFileNewHeaderFile();
    void OnEditCopy();
    void OnEditPaste();
    void OnEditCut();
    void OnEditSearch();
    void Search(bool forward);
  private:
    bool is_new_file;
    Gtk::TextIter search_match_end_;
    Gtk::TextIter search_match_start_;
  };  // class controller
}  // namespace Notebook


#endif  // JUCI_NOTEBOOK_H_
