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
    Gtk::HBox& view() {return view_;}
    Gtk::Notebook& notebook() {return notebook_; }
    Gtk::Label& linenumbers() {return linenumbers_;}
    Gtk::ScrolledWindow text, line;
    
  protected:
    Gtk::HBox view_;
    Gtk::Notebook notebook_;
    Gtk::Label linenumbers_;
  };
  class Controller {
  public:
    Gtk::ScrolledWindow linesscroll;
    Controller(Keybindings::Controller& keybindings);
    ~Controller();
    Glib::RefPtr<Gtk::TextBuffer> Buffer();
    int CurrentPage();
    Gtk::Box& entry_view();
    Gtk::Notebook& Notebook();
    Gtk::Label& Linenumbers();
    void OnBufferChange();
    void OnCloseCurrentPage();
    std::string GetCursorWord();
    void OnEditCopy();
    void OnEditCut();
    void OnEditPaste();
    void OnEditSearch();
    void OnFileNewCCFile();
    void OnFileNewEmptyfile();
    void OnFileNewHeaderFile();
    void OnNewPage(std::string name);
    void OnOpenFile(std::string filename);
    void OnCreatePage();
    bool scroll_event_callback(GdkEventScroll* scroll_event);
    int Pages();
    Gtk::HBox& view();
    void Search(bool forward);
    View view_;
    Model model_;
    bool is_new_file_;
    Entry::Controller entry_;
    std::vector<Source::Controller*> source_vec_, linenumbers;
    std::vector<Gtk::Paned*> paned;
    std::vector<Gtk::ScrolledWindow*> scrolledwindow_vec_, scrolledeverything_vec_;
    std::vector<Gtk::Label*> label_vec_;
    std::vector<Gtk::HBox*> box_l, box_h, box_m;
    std::list<Gtk::TargetEntry> listTargets_;
    Gtk::TextIter search_match_end_;
    Gtk::TextIter search_match_start_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard_;
    
  protected:
    void BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer> buffer);
  };  // class controller
}  // namespace Notebook


#endif  // JUCI_NOTEBOOK_H_
