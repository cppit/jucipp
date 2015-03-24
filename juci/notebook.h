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
    std::string cc_extension_;
    std::string h_extension_;
    int scrollvalue_;
  };
  class View {
  public:
    View();
    Gtk::HBox& view() {return view_;}
    Gtk::Notebook& notebook() {return notebook_; }
    Gtk::ScrolledWindow text, line;
    
  protected:
    Gtk::HBox view_;
    Gtk::Notebook notebook_;
  };
  class Controller {
  public:
    Controller(Keybindings::Controller& keybindings);
    ~Controller();
    Glib::RefPtr<Gtk::TextBuffer> Buffer( Source::Controller *source);
    Gtk::TextView& CurrentTextView();
    int CurrentPage();
    Gtk::Box& entry_view();
    Gtk::Notebook& Notebook();
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
  protected:
    void BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer> buffer);
  private:
    View view_;
    Model model_;
    bool is_new_file_;
    Entry::Controller entry_;
    std::vector<Source::Controller*> text_vec_, linenumbers_vec_;
    std::vector<Gtk::ScrolledWindow*> scrolledtext_vec_, scrolledline_vec_;
    std::vector<Gtk::HBox*> editor_vec_;
    std::list<Gtk::TargetEntry> listTargets_;
    Gtk::TextIter search_match_end_;
    Gtk::TextIter search_match_start_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard_;
    std::string
    lines="1 ";
    
    
  };  // class controller
}  // namespace Notebook




#endif  // JUCI_NOTEBOOK_H_
