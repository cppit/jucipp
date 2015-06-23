#include <fstream>
#include "notebook.h"
#include "logging.h"

Notebook::Model::Model() {
  cc_extension_ = ".cpp";
  h_extension_  = ".hpp";
  scrollvalue_ = 50;
}

Notebook::View::View() : notebook_() {
  view_.pack2(notebook_);
  view_.set_position(120);
}

Notebook::Controller::Controller(Gtk::Window* window,
                                 Keybindings::Controller& keybindings,
                                 Source::Config& source_cfg,
                                 Directories::Config& dir_cfg) :
  directories_(dir_cfg),
  source_config_(source_cfg) {
  INFO("Create notebook");
  window_ = window;
  refClipboard_ = Gtk::Clipboard::get();
  view().pack1(directories_.widget(), true, true);
  CreateKeybindings(keybindings);
  INFO("Notebook Controller Success");
}  // Constructor


void Notebook::Controller::CreateKeybindings(Keybindings::Controller
                                             &keybindings) {
  INFO("Notebook create signal handlers");
  directories().m_TreeView.signal_row_activated()
    .connect(sigc::mem_fun(*this,
                           &Notebook::Controller::OnDirectoryNavigation));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileMenu",
                            Gtk::Stock::FILE));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewStandard",                     
                            "New empty file"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["new_file"]),
        [this]() {
          is_new_file_ = true;
          OnFileNewEmptyfile();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewCC",
                            "New source file"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["new_cc_file"]),
        [this]() {
          is_new_file_ = true;
          OnFileNewCCFile();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewH",
                            "New header file"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["new_h_file"]),
        [this]() {
          is_new_file_ = true;
          OnFileNewHeaderFile();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("WindowCloseTab",
                            "Close tab"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["close_tab"]),
        [this]() {
          OnCloseCurrentPage();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditFind",
                            "Find"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_find"]),
        [this]() {
          is_new_file_ = false;
          OnEditSearch();
          // TODO(Oyvang)  Zalox, Forgi)Create function OnEditFind();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditCopy",
                            "Copy"),
         Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_copy"]),
       
        [this]() {
          OnEditCopy();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditCut",
                            "Cut"),
         Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_cut"]),
        [this]() {
          OnEditCut();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditPaste",
                            "Paste"),
         Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_paste"]),
        [this]() {
          OnEditPaste();
        });

  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditUndo",
                            "Undo"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_undo"]),
        [this]() {
	  INFO("On undo");
          Glib::RefPtr<Gsv::UndoManager> undo_manager =
	    CurrentTextView().get_source_buffer()->get_undo_manager();
          if (Pages() != 0 && undo_manager->can_undo()) {
            undo_manager->undo();
          }
          INFO("Done undo");
        }
	);

  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditRedo",
                            "Redo"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["edit_redo"]),
        [this]() {
          INFO("On Redo");
          Glib::RefPtr<Gsv::UndoManager> undo_manager =
          CurrentTextView().get_source_buffer()->get_undo_manager();
          if (Pages() != 0 && undo_manager->can_redo()) {
            undo_manager->redo();
          }
          INFO("Done Redo");
        });

  entry_.view_.entry().signal_activate().
    connect(
            [this]() {
              if (is_new_file_) {
                //OnNewPage(entry_.text()); //TODO: rewrite new file (the file needs to be created before opened with Source::Controller in order to choose the correct view implementation)
                entry_.OnHideEntries(is_new_file_);
              } else {
                Search(true);
              }
            });
  entry_.button_apply().signal_clicked().
    connect(
            [this]() {
              //OnNewPage(entry_.text()); //TODO: rewrite new file (the file needs to be created before opened with Source::Controller in order to choose the correct view implementation)
              entry_.OnHideEntries(is_new_file_);
            });
  entry_.button_close().signal_clicked().
    connect(
            [this]() {
              entry_.OnHideEntries(is_new_file_);
            });
  entry_.button_next().signal_clicked().
    connect(
            [this]() {
              Search(true);
            });
  entry_.button_prev().signal_clicked().
    connect(
            [this]() {
              Search(false);
            });
  INFO("Notebook signal handlers sucsess");
}

Notebook::Controller::~Controller() {
  INFO("Notebook destructor");
  for (auto &i : editor_vec_) delete i;
  for (auto &i : scrolledtext_vec_) delete i;
}

Gtk::Paned& Notebook::Controller::view() {
  return view_.view();
}
Gtk::Box& Notebook::Controller::entry_view() {
  return entry_.view();
}

void Notebook::Controller::OnOpenFile(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  text_vec_.emplace_back(new Source::Controller(source_config(), path, project_path));
  scrolledtext_vec_.push_back(new Gtk::ScrolledWindow());
  editor_vec_.push_back(new Gtk::HBox());
  scrolledtext_vec_.back()->add(*text_vec_.back()->view);
  editor_vec_.back()->pack_start(*scrolledtext_vec_.back(), true, true);
  size_t pos = path.find_last_of("/\\");
  std::string filename=path;
  if(pos!=std::string::npos)
    filename=path.substr(pos+1);
  Notebook().append_page(*editor_vec_.back(), filename);
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  Notebook().set_focus_child(*text_vec_.back()->view);
  set_source_handlers(*text_vec_.back());
}

void Notebook::Controller::OnCloseCurrentPage() {
  INFO("Notebook close page");
  if (Pages() != 0) {
    if(!text_vec_.at(CurrentPage())->is_saved){
      AskToSaveDialog();
    }
    int page = CurrentPage();
    Notebook().remove_page(page);
    delete scrolledtext_vec_.at(page);
    delete editor_vec_.at(page);
    text_vec_.erase(text_vec_.begin()+ page);
    scrolledtext_vec_.erase(scrolledtext_vec_.begin()+page);
    editor_vec_.erase(editor_vec_.begin()+page);
  }
}
void Notebook::Controller::OnFileNewEmptyfile() {
  entry_.OnShowSetFilenName("");
}
void Notebook::Controller::OnFileNewCCFile() {
  entry_.OnShowSetFilenName(model_.cc_extension_);
}
void Notebook::Controller::OnFileNewHeaderFile() {
  entry_.OnShowSetFilenName(model_.h_extension_);
}
void Notebook::Controller::OnEditCopy() {
  if (Pages() != 0) {
    Buffer(*text_vec_.at(CurrentPage()))->copy_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditPaste() {
  if (Pages() != 0) {
    Buffer(*text_vec_.at(CurrentPage()))->paste_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditCut() {
  if (Pages() != 0) {
    Buffer(*text_vec_.at(CurrentPage()))->cut_clipboard(refClipboard_);
  }
}

std::string Notebook::Controller::GetCursorWord() {
  INFO("Notebook get cursor word");
  int page = CurrentPage();
  std::string word;
  Gtk::TextIter start, end;
  start = Buffer(*text_vec_.at(page))->get_insert()->get_iter();
  end = Buffer(*text_vec_.at(page))->get_insert()->get_iter();
  if (!end.ends_line()) {
    while (!end.ends_word()) {
      end.forward_char();
    }
  }
  if (!start.starts_line()) {
    while (!start.starts_word()) {
      start.backward_char();
    }
  }
  word = Buffer(*text_vec_.at(page))->get_text(start, end);
  // TODO(Oyvang) fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch() {
  search_match_end_ =
    Buffer(*text_vec_.at(CurrentPage()))->get_iter_at_offset(0);
  entry_.OnShowSearch(GetCursorWord());
}

void Notebook::Controller::Search(bool forward) {
    INFO("Notebook search");
  int page = CurrentPage();
  std::string search_word;
  search_word = entry_.text();
  Gtk::TextIter test;

  if ( !forward ) {
    if ( search_match_start_ == 0 ||
         search_match_start_.get_line_offset() == 0) {
      search_match_start_ = Buffer(*text_vec_.at(CurrentPage()))->end();
    }
    search_match_start_.
      backward_search(search_word,
                      Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
                      Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
                      search_match_start_,
                      search_match_end_);
  } else {
    if ( search_match_end_ == 0 ) {
      search_match_end_ = Buffer(*text_vec_.at(CurrentPage()))->begin();
    }
    search_match_end_.
      forward_search(search_word,
                     Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
                     Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
                     search_match_start_,
                     search_match_end_);
  }
}

void Notebook::Controller
::OnDirectoryNavigation(const Gtk::TreeModel::Path& path,
                        Gtk::TreeViewColumn* column) {
    INFO("Notebook directory navigation");
  Gtk::TreeModel::iterator iter = directories().m_refTreeModel->get_iter(path);
  if (iter) {
    Gtk::TreeModel::Row row = *iter;
    std::string upath = Glib::ustring(row[directories().view().m_col_path]);
    boost::filesystem::path fs_path(upath);
    if (boost::filesystem::is_directory(fs_path)) {
      directories().m_TreeView.row_expanded(path) ?
        directories().m_TreeView.collapse_row(path) :
        directories().m_TreeView.expand_row(path, false);
    } else {
      std::stringstream sstm;
      sstm << row[directories().view().m_col_path];
      std::string file = sstm.str();
      OnOpenFile(file);
    }
  }
}

Source::View& Notebook::Controller::CurrentTextView() {
  INFO("Getting sourceview");
  return *text_vec_.at(CurrentPage())->view;
}

int Notebook::Controller::CurrentPage() {
  return Notebook().get_current_page();
}

Glib::RefPtr<Gtk::TextBuffer>
Notebook::Controller::Buffer(Source::Controller &source) {
  return source.view->get_buffer();
}

int Notebook::Controller::Pages() {
  return Notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::Notebook() {
  return view_.notebook();
}

void Notebook::Controller::BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer>
                                               buffer) {
  buffer->signal_end_user_action().connect(
                                   [this]() {                                     
                                     //UpdateHistory();
                                   });
}

void Notebook::Controller::set_source_handlers(Source::Controller& controller) {
  //Add star on tab label when the page is not saved:
  controller.buffer()->signal_changed().connect([this]() {
    if(text_vec_.at(CurrentPage())->is_saved) {
      std::string path=text_vec_.at(CurrentPage())->view->file_path;
      size_t pos = path.find_last_of("/\\");
      std::string filename=path;
      if(pos!=std::string::npos)
        filename=path.substr(pos+1);
      Notebook().set_tab_label_text(*(Notebook().get_nth_page(CurrentPage())), filename+"*");
    }
    text_vec_.at(CurrentPage())->is_saved=false;
  });
}

std::string Notebook::Controller::CurrentPagePath(){
  return text_vec_.at(CurrentPage())->view->file_path;
}

bool Notebook::Controller:: OnSaveFile() {
  std::string path=text_vec_.at(CurrentPage())->view->file_path;
  return OnSaveFile(path);
}
bool Notebook::Controller:: OnSaveFile(std::string path) {
    INFO("Notebook save file with path");
    if (path != "") {
      std::ofstream file;
      file.open (path);
      file << CurrentTextView().get_buffer()->get_text();
      file.close();
      text_vec_.at(CurrentPage())->view->file_path=path;
      size_t pos = path.find_last_of("/\\");
      std::string filename=path;
      if(pos!=std::string::npos)
        filename=path.substr(pos+1);
      Notebook().set_tab_label_text(*Notebook().get_nth_page(CurrentPage()), filename);
      text_vec_.at(CurrentPage())->is_saved=true;
      return true;
    }
  return false;
}


std::string Notebook::Controller::OnSaveFileAs(){
  INFO("Notebook save as");
  Gtk::FileChooserDialog dialog("Please choose a file", 
				Gtk::FILE_CHOOSER_ACTION_SAVE);
  DEBUG("SET TRANSISTEN FPR");
  dialog.set_transient_for(*window_);
  dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Save", Gtk::RESPONSE_OK);
  //dialog.set_current_name("Untitled");
  DEBUG("RUN DIALOG");
  int result = dialog.run();
  DEBUG("DIALOG RUNNING");
  switch (result) {
  case(Gtk::RESPONSE_OK): {
    DEBUG("get_filename()");
    std::string path = dialog.get_filename();
    unsigned pos = path.find_last_of("/\\");
     return path;
  }
  case(Gtk::RESPONSE_CANCEL): {
    break;
  }
  default: {
    DEBUG("Unexpected button clicked."); 
    break;
  }
  }
  return "";
}

void Notebook::Controller::AskToSaveDialog() {
  INFO("AskToSaveDialog");
  DEBUG("AskToSaveDialog: Finding file path");
  Gtk::MessageDialog dialog(*window_, "Save file!",
			    false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_secondary_text(
          "Do you want to save: " +
			    text_vec_.at(CurrentPage())->view->file_path+" ?");
  DEBUG("AskToSaveDialog: run dialog");
  int result = dialog.run();

  //Handle the response:
  DEBUG("AskToSaveDialog: switch response");
  switch(result)
  {
    case(Gtk::RESPONSE_YES):
    {
      DEBUG("AskToSaveDialog: save file: yes, trying to save file");
      OnSaveFile();
      DEBUG("AskToSaveDialog: save file: yes, saved sucess");
      break;
    }
    case(Gtk::RESPONSE_NO):
    {
      DEBUG("AskToSaveDialog: save file: no");
      break;
    }
    default:
    {
           DEBUG("AskToSaveDialog: unexpected action: Default switch");
      break;
    }
  }
}

