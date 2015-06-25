#include <fstream>
#include "notebook.h"
#include "logging.h"

Notebook::View::View() : notebook_() {
  view_.pack2(notebook_);
  view_.set_position(120);
}

Notebook::Controller::Controller(Gtk::Window* window,
                                 Keybindings::Controller& keybindings,
                                 Source::Config& source_cfg,
                                 Directories::Config& dir_cfg) :
  directories(dir_cfg),
  source_config(source_cfg) {
  INFO("Create notebook");
  window_ = window;
  refClipboard_ = Gtk::Clipboard::get();
  view().pack1(directories.widget(), true, true);
  CreateKeybindings(keybindings);
  INFO("Notebook Controller Success");
}  // Constructor


void Notebook::Controller::CreateKeybindings(Keybindings::Controller
                                             &keybindings) {
  INFO("Notebook create signal handlers");
  directories.m_TreeView.signal_row_activated()
    .connect(sigc::mem_fun(*this,
                           &Notebook::Controller::OnDirectoryNavigation));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileMenu",
                            Gtk::Stock::FILE));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewFile",                     
                            "New file"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["new_file"]),
        [this]() {
          OnFileNewFile();
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

  entry.button_apply_set_filename.signal_clicked().connect([this]() {
    std::string filename=entry();
    if(filename!="") {
      if(project_path!="" && !boost::filesystem::path(filename).is_absolute())
        filename=project_path+"/"+filename;
      boost::filesystem::path p(filename);
      if(boost::filesystem::exists(p)) {
        //TODO: alert user that file already exists
      }
      else {
        std::ofstream f(p.string().c_str());
        if(f) {
          OnOpenFile(boost::filesystem::canonical(p).string());
          if(project_path!="")
            directories.open_folder(project_path); //TODO: Do refresh instead
        }
        else {
          //TODO: alert user of error creating file
        }
        f.close();
      }
    }
    entry.hide();
  });
  entry.button_close.signal_clicked().
    connect(
            [this]() {
              entry.hide();
            });
  entry.button_next.signal_clicked().
    connect(
            [this]() {
              Search(true);
            });
  entry.button_prev.signal_clicked().
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

void Notebook::Controller::OnOpenFile(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  text_vec_.emplace_back(new Source::Controller(source_config, path, project_path));
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
  //Add star on tab label when the page is not saved:
  text_vec_.back()->buffer()->signal_changed().connect([this]() {
    if(text_vec_.at(CurrentPage())->is_saved) {
      std::string path=CurrentTextView().file_path;
      size_t pos = path.find_last_of("/\\");
      std::string filename=path;
      if(pos!=std::string::npos)
        filename=path.substr(pos+1);
      Notebook().set_tab_label_text(*(Notebook().get_nth_page(CurrentPage())), filename+"*");
    }
    text_vec_.at(CurrentPage())->is_saved=false;
  });
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
void Notebook::Controller::OnFileNewFile() {
  entry.show_set_filename();
}
void Notebook::Controller::OnEditCopy() {
  if (Pages() != 0) {
    CurrentTextView().get_buffer()->copy_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditPaste() {
  if (Pages() != 0) {
    CurrentTextView().get_buffer()->paste_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditCut() {
  if (Pages() != 0) {
    CurrentTextView().get_buffer()->cut_clipboard(refClipboard_);
  }
}

std::string Notebook::Controller::GetCursorWord() {
  INFO("Notebook get cursor word");
  int page = CurrentPage();
  std::string word;
  Gtk::TextIter start, end;
  start = CurrentTextView().get_buffer()->get_insert()->get_iter();
  end = CurrentTextView().get_buffer()->get_insert()->get_iter();
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
  word = CurrentTextView().get_buffer()->get_text(start, end);
  // TODO(Oyvang) fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch() {
  search_match_end_ =
    CurrentTextView().get_buffer()->get_iter_at_offset(0);
  entry.show_search(GetCursorWord());
}

void Notebook::Controller::Search(bool forward) {
    INFO("Notebook search");
  int page = CurrentPage();
  std::string search_word;
  search_word = entry();
  Gtk::TextIter test;

  if ( !forward ) {
    if ( search_match_start_ == 0 ||
         search_match_start_.get_line_offset() == 0) {
      search_match_start_ = CurrentTextView().get_buffer()->end();
    }
    search_match_start_.
      backward_search(search_word,
                      Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
                      Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
                      search_match_start_,
                      search_match_end_);
  } else {
    if ( search_match_end_ == 0 ) {
      search_match_end_ = CurrentTextView().get_buffer()->begin();
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
  Gtk::TreeModel::iterator iter = directories.m_refTreeModel->get_iter(path);
  if (iter) {
    Gtk::TreeModel::Row row = *iter;
    std::string upath = Glib::ustring(row[directories.view().m_col_path]);
    boost::filesystem::path fs_path(upath);
    if (boost::filesystem::is_directory(fs_path)) {
      directories.m_TreeView.row_expanded(path) ?
        directories.m_TreeView.collapse_row(path) :
        directories.m_TreeView.expand_row(path, false);
    } else {
      std::stringstream sstm;
      sstm << row[directories.view().m_col_path];
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

int Notebook::Controller::Pages() {
  return Notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::Notebook() {
  return view_.notebook();
}

bool Notebook::Controller:: OnSaveFile() {
  std::string path=CurrentTextView().file_path;
  return OnSaveFile(path);
}
bool Notebook::Controller:: OnSaveFile(std::string path) {
    INFO("Notebook save file with path");
    if (path != "") {
      std::ofstream file;
      file.open (path);
      file << CurrentTextView().get_buffer()->get_text();
      file.close();
      CurrentTextView().file_path=path;
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
			    CurrentTextView().file_path+" ?");
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

