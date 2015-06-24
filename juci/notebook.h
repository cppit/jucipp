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
    Gtk::Paned& view() {return view_;}
    Gtk::Notebook& notebook() {return notebook_; }
  protected:
    Gtk::Paned view_;
    Gtk::Notebook notebook_;
  };
  class Controller {
  public:
    Controller(Gtk::Window* window, Keybindings::Controller& keybindings,
               Source::Config& config,
               Directories::Config& dir_cfg);
    ~Controller();
    Glib::RefPtr<Gtk::TextBuffer> Buffer(Source::Controller &source);
    Source::View& CurrentTextView();
    int CurrentPage();
    Gtk::Box& entry_view();
    Gtk::Notebook& Notebook();
    std::string CurrentPagePath();
    void OnBufferChange();
    void BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer>
                                               buffer);
    void OnCloseCurrentPage();
    std::string GetCursorWord();
    void OnEditCopy();
    void OnEditCut();
    void OnEditPaste();
    void OnEditSearch();
    void OnFileNewCCFile();
    void OnFileNewEmptyfile();
    void OnFileNewHeaderFile();
    void OnFileOpenFolder();
    bool OnSaveFile();
    bool OnSaveFile(std::string path);
    void OnDirectoryNavigation(const Gtk::TreeModel::Path& path,
                               Gtk::TreeViewColumn* column);
    void OnOpenFile(std::string filename);
    bool ScrollEventCallback(GdkEventScroll* scroll_event);
    int Pages();
    Gtk::Paned& view();
    void Search(bool forward);
    std::string OnSaveFileAs();
    std::string project_path;
    Directories::Controller directories;
  private:
    void CreateKeybindings(Keybindings::Controller& keybindings);
    void AskToSaveDialog();
    Glib::RefPtr<Gtk::Builder> m_refBuilder;
    Glib::RefPtr<Gio::SimpleActionGroup> refActionGroup;
    Source::Config& source_config;
    View view_;
    Model model_;
    bool is_new_file_; //TODO: Remove this
    Entry::Controller entry_;

    std::vector<std::unique_ptr<Source::Controller> > text_vec_;
    std::vector<Gtk::ScrolledWindow*> scrolledtext_vec_;
    std::vector<Gtk::HBox*> editor_vec_;
    std::list<Gtk::TargetEntry> listTargets_;
    Gtk::TextIter search_match_end_;
    Gtk::TextIter search_match_start_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard_;
    Gtk::Window* window_;
  };  // class controller
}  // namespace Notebook
#endif  // JUCI_NOTEBOOK_H_
