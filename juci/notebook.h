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
#include "keybindings.h"
#include "terminal.h"

namespace Notebook {
  class View : public Gtk::Paned {
  public:
    View();
    Gtk::Notebook notebook;
  };
  class Controller {
  public:
    Controller(Keybindings::Controller& keybindings,
               Terminal::Controller& terminal,
               Source::Config& config,
               Directories::Config& dir_cfg);
    ~Controller();
    Source::View& CurrentTextView();
    int CurrentPage();
    Gtk::Notebook& Notebook();
    void OnCloseCurrentPage();
    std::string GetCursorWord();
    void OnEditCopy();
    void OnEditCut();
    void OnEditPaste();
    void OnEditSearch();
    void OnFileNewFile();
    bool OnSaveFile();
    bool OnSaveFile(std::string path);
    void OnDirectoryNavigation(const Gtk::TreeModel::Path& path,
                               Gtk::TreeViewColumn* column);
    void OnOpenFile(std::string filename);
    int Pages();
    void Search(bool forward);
    View view;
    std::string OnSaveFileAs();
    std::string project_path;
    Directories::Controller directories; //Todo: make private after creating open_directory()
    Entry entry;
  private:
    void CreateKeybindings(Keybindings::Controller& keybindings);
    void AskToSaveDialog();
    Glib::RefPtr<Gtk::Builder> m_refBuilder;
    Glib::RefPtr<Gio::SimpleActionGroup> refActionGroup;
    Terminal::Controller& terminal;
    Source::Config& source_config;

    std::vector<std::unique_ptr<Source::Controller> > text_vec_;
    std::vector<Gtk::ScrolledWindow*> scrolledtext_vec_;
    std::vector<Gtk::HBox*> editor_vec_;
    std::list<Gtk::TargetEntry> listTargets_;
    Gtk::TextIter search_match_end_;
    Gtk::TextIter search_match_start_;
    Glib::RefPtr<Gtk::Clipboard> refClipboard_;
  };  // class controller
}  // namespace Notebook
#endif  // JUCI_NOTEBOOK_H_
