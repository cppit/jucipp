#include "notebook.h"
#include <fstream>

Notebook::Model::Model() {
  cc_extension_ = ".cc";
  h_extension_  = ".h";
  scrollvalue_ = 50;
}

Notebook::View::View() : notebook_() {
  view_.pack2(notebook_);
  view_.set_position(120);
}

Notebook::Controller::Controller(Gtk::Window* window, Keybindings::Controller& keybindings,
                                 Source::Config& source_cfg,
                                 Directories::Config& dir_cfg) :
  source_config_(source_cfg),
  directories_(dir_cfg),
  index_(0, 1) {
  window_ = window;
  OnNewPage("juCi++");
  refClipboard_ = Gtk::Clipboard::get();
  ispopup = false;
  view().pack1(directories_.widget(), true, true);
  CreateKeybindings(keybindings);
}  // Constructor


void Notebook::Controller::CreateKeybindings(Keybindings::Controller
                                             &keybindings) {
  directories().m_TreeView.signal_row_activated()
    .connect(sigc::mem_fun(*this,
                           &Notebook::Controller::OnDirectoryNavigation));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileMenu",
                            Gtk::Stock::FILE));

  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewStandard",
                            Gtk::Stock::NEW,
                            "New empty file",
                            "Create a new file"),
        [this]() {
          is_new_file_ = true;
          OnFileNewEmptyfile();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewCC",
                            "New cc file"),
        Gtk::AccelKey(keybindings.config_
                      .key_map()["new_cc_file"]),
        [this]() {
          is_new_file_ = true;
          OnFileNewCCFile();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("FileNewH",
                            "New h file"),
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
                            Gtk::Stock::FIND),
        [this]() {
          is_new_file_ = false;
          OnEditSearch();
          // TODO(Oyvang)  Zalox, Forgi)Create function OnEditFind();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditCopy",
                            Gtk::Stock::COPY),
        [this]() {
          OnEditCopy();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditCut",
                            Gtk::Stock::CUT),
        [this]() {
          OnEditCut();
        });
  keybindings.action_group_menu()->
    add(Gtk::Action::create("EditPaste",
                            Gtk::Stock::PASTE),
        [this]() {
          OnEditPaste();
        });
  entry_.view_.entry().signal_activate().
    connect(
            [this]() {
              if (is_new_file_) {
                OnNewPage(entry_.text());
                entry_.OnHideEntries(is_new_file_);
              } else {
                Search(true);
              }
            });
  entry_.button_apply().signal_clicked().
    connect(
            [this]() {
              OnNewPage(entry_.text());
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
}

bool Notebook::Controller:: OnMouseRelease(GdkEventButton* button) {
  if (button->button == 1 && ispopup) {
    popup_.response(Gtk::RESPONSE_DELETE_EVENT);
    return true;
  }
  return false;
}

bool Notebook::Controller::OnKeyRelease(GdkEventKey* key) {
  return GeneratePopup(key->keyval);
}

bool Notebook::Controller::GeneratePopup(int key_id) {
  //  Get function to fill popup with suggests item vector under is for testing
  Gtk::TextIter beg = CurrentTextView().get_buffer()->get_insert()->get_iter();
  Gtk::TextIter end = CurrentTextView().get_buffer()->get_insert()->get_iter();
  Gtk::TextIter tmp = CurrentTextView().get_buffer()->get_insert()->get_iter();
  Gtk::TextIter tmp1 = CurrentTextView().get_buffer()->get_insert()->get_iter();
  Gtk::TextIter line =
  CurrentTextView().get_buffer()->get_iter_at_line(tmp.get_line());
  if (end.backward_char() && end.backward_char()) {
    bool illegal_chars =
    end.backward_search("\"", Gtk::TEXT_SEARCH_VISIBLE_ONLY, tmp, tmp1, line)
    ||
      end.backward_search("//", Gtk::TEXT_SEARCH_VISIBLE_ONLY, tmp, tmp1, line);
      if (illegal_chars) {
        return false;
      }
      std::string c = text_vec_[CurrentPage()]->buffer()->get_text(end, beg);
      switch (key_id) {
      case 46:
        break;
      case 58:
        if (c != "::") return false;
        break;
      case 60:
        if (c != "->") return false;
        break;
      case 62:
        if (c != "->") return false;
        break;
      default:
        return false;
      }
  } else {
    return false;
  }
  std::vector<Source::AutoCompleteData> acdata;
  text_vec_.at(CurrentPage())->
    GetAutoCompleteSuggestions(beg.get_line()+1,
                               beg.get_line_offset()+2,
                               &acdata);
  std::map<std::string, std::string> items;
  for (auto &data : acdata) {
    std::stringstream ss;
    std::string return_value;
    for (auto &chunk : data.chunks_) {
      switch (chunk.kind()) {
      case clang::CompletionChunk_ResultType:
        return_value = chunk.chunk();
        break;
      case clang::CompletionChunk_Informative:
        break;
      default:
        ss << chunk.chunk();
        break;
      }
    }
    items[ss.str() + " --> " + return_value] = ss.str();
  }
  //  Replace over with get suggestions from zalox! OVER IS JUST FOR TESTING
  Gtk::ScrolledWindow popup_scroll_;
  Gtk::ListViewText listview_(1, false, Gtk::SelectionMode::SELECTION_SINGLE);
  popup_scroll_.set_policy(Gtk::PolicyType::POLICY_NEVER,
                           Gtk::PolicyType::POLICY_NEVER);
  listview_.set_enable_search(true);
  listview_.set_headers_visible(false);
  listview_.set_hscroll_policy(Gtk::ScrollablePolicy::SCROLL_NATURAL);
  listview_.set_activate_on_single_click(true);
  if (items.empty()) {
    items["No suggestions found..."] = "";
  }
  for (auto &i : items) {
    listview_.append(i.first);
  }
  popup_scroll_.add(listview_);
  popup_.get_vbox()->pack_start(popup_scroll_);
  popup_.set_transient_for(*window_);
  popup_.show_all();

  int popup_x = popup_.get_width();
  int popup_y = items.size() * 20;
  PopupSetSize(popup_scroll_, popup_x, popup_y);
  int x, y;
  FindPopupPosition(CurrentTextView(), popup_x, popup_y, x, y);
  popup_.move(x, y+15);
  PopupSelectHandler(popup_, listview_, &items);
  ispopup = true;
  popup_.run();
  popup_.hide();
  ispopup = false;
  return true;
}

bool Notebook::Controller::ScrollEventCallback(GdkEventScroll* scroll_event) {
  int page = CurrentPage();
  int direction_y = scroll_event->delta_y;
  int direction_x = scroll_event->delta_x;

  Glib::RefPtr<Gtk::Adjustment> adj =
    scrolledtext_vec_.at(page)->
    get_vscrollbar()->get_adjustment();
  if ( direction_y != 0 ) {
    int dir_val = direction_y == -1 ? -model_.scrollvalue_:+model_.scrollvalue_;
    adj->set_value(adj->get_value()+dir_val);
    text_vec_.at(page)->view().set_vadjustment(adj);
    linenumbers_vec_.at(page)->view().set_vadjustment(adj);
  }
  return true;
}
Notebook::Controller::~Controller() {
  for (auto &i : text_vec_) delete i;
  for (auto &i : linenumbers_vec_) delete i;
  for (auto &i : editor_vec_) delete i;
  for (auto &i : scrolledtext_vec_) delete i;
  for (auto &i : scrolledline_vec_) delete i;
}

Gtk::Paned& Notebook::Controller::view() {
  return view_.view();
}
Gtk::Box& Notebook::Controller::entry_view() {
  return entry_.view();
}

void Notebook::Controller::OnNewPage(std::string name) {
  OnCreatePage();
  text_vec_.back()->OnNewEmptyFile();
  Notebook().append_page(*editor_vec_.back(), name);
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  Notebook().set_focus_child(text_vec_.at(Pages()-1)->view());
}

void Notebook::Controller::
MapBuffers(std::map<std::string, std::string> *buffers) {
  for (auto &buffer : text_vec_) {
    buffers->operator[](buffer->model().file_path()) =
      buffer->buffer()->get_text().raw();
  }
}

void Notebook::Controller::OnOpenFile(std::string path) {
  OnCreatePage();
  text_vec_.back()->OnOpenFile(path);
  text_vec_.back()->set_is_saved(true);
  unsigned pos = path.find_last_of("/\\");
  Notebook().append_page(*editor_vec_.back(), path.substr(pos+1));
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  Notebook().set_focus_child(text_vec_.back()->view());
  OnBufferChange();
}

void Notebook::Controller::OnCreatePage() {
  text_vec_.push_back(new Source::Controller(source_config(), this));
  linenumbers_vec_.push_back(new Source::Controller(source_config(), this));
  scrolledline_vec_.push_back(new Gtk::ScrolledWindow());
  scrolledtext_vec_.push_back(new Gtk::ScrolledWindow());
  editor_vec_.push_back(new Gtk::HBox());
  scrolledtext_vec_.back()->add(text_vec_.back()->view());
  scrolledline_vec_.back()->add(linenumbers_vec_.back()->view());
  linenumbers_vec_.back()->view().get_buffer()->set_text("1 ");
  linenumbers_vec_.back()->view().override_color(Gdk::RGBA("Black"));
  linenumbers_vec_.back()->
    view().set_justification(Gtk::Justification::JUSTIFY_RIGHT);
  scrolledline_vec_.back()->get_vscrollbar()->hide();
  linenumbers_vec_.back()->view().set_editable(false);
  linenumbers_vec_.back()->view().set_sensitive(false);
  editor_vec_.back()->pack_start(*scrolledline_vec_.back(), false, false);
  editor_vec_.back()->pack_start(*scrolledtext_vec_.back(), true, true);
  TextViewHandlers(text_vec_.back()->view());
}

void Notebook::Controller::OnCloseCurrentPage() {
  // TODO(oyvang) zalox, forgi)
  // Save a temp file, in case you close one you dont want to close?
  if (Pages() != 0) {
    int page = CurrentPage();
    Notebook().remove_page(page);
    delete text_vec_.at(page);
    delete linenumbers_vec_.at(page);
    delete scrolledtext_vec_.at(page);
    delete scrolledline_vec_.at(page);
    delete editor_vec_.at(page);
    text_vec_.erase(text_vec_.begin()+ page);
    linenumbers_vec_.erase(linenumbers_vec_.begin()+page);
    scrolledtext_vec_.erase(scrolledtext_vec_.begin()+page);
    scrolledline_vec_.erase(scrolledline_vec_.begin()+page);
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
    Buffer(text_vec_.at(CurrentPage()))->copy_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditPaste() {
  if (Pages() != 0) {
    Buffer(text_vec_.at(CurrentPage()))->paste_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditCut() {
  if (Pages() != 0) {
    Buffer(text_vec_.at(CurrentPage()))->cut_clipboard(refClipboard_);
  }
}

std::string Notebook::Controller::GetCursorWord() {
  int page = CurrentPage();
  std::string word;
  Gtk::TextIter start, end;
  start = Buffer(text_vec_.at(page))->get_insert()->get_iter();
  end = Buffer(text_vec_.at(page))->get_insert()->get_iter();
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
  word = Buffer(text_vec_.at(page))->get_text(start, end);
  // TODO(Oyvang) fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch() {
  search_match_end_ =
    Buffer(text_vec_.at(CurrentPage()))->get_iter_at_offset(0);
  entry_.OnShowSearch(GetCursorWord());
}

void Notebook::Controller::Search(bool forward) {
  int page = CurrentPage();
  std::string search_word;
  search_word = entry_.text();
  Gtk::TextIter test;

  if ( !forward ) {
    if ( search_match_start_ == 0 ||
         search_match_start_.get_line_offset() == 0) {
      search_match_start_ = Buffer(text_vec_.at(CurrentPage()))->end();
    }
    search_match_start_.
      backward_search(search_word,
                      Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
                      Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
                      search_match_start_,
                      search_match_end_);
  } else {
    if ( search_match_end_ == 0 ) {
      search_match_end_ = Buffer(text_vec_.at(CurrentPage()))->begin();
    }
    search_match_end_.
      forward_search(search_word,
                     Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
                     Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
                     search_match_start_,
                     search_match_end_);
  }
}

void Notebook::Controller::OnBufferChange() {
  int page =  CurrentPage();
  int text_nr = Buffer(text_vec_.at(page))->get_line_count();
  int line_nr =  Buffer(linenumbers_vec_.at(page))->get_line_count();
  while (line_nr < text_nr) {
    line_nr++;
    Buffer(linenumbers_vec_.at(page))->
      insert(Buffer(linenumbers_vec_.at(page))->end(),
             "\n"+std::to_string(line_nr)+" ");
  }
  while (line_nr > text_nr) {
    Gtk::TextIter iter =
      Buffer(linenumbers_vec_.at(page))->get_iter_at_line(line_nr);
    iter.backward_char();
    line_nr--;
    Buffer(linenumbers_vec_.at(page))->
      erase(iter,
            Buffer(linenumbers_vec_.at(page))->end());
  }
  if (Buffer(text_vec_.at(page))->get_insert()->get_iter().starts_line() &&
      Buffer(text_vec_.at(page))->get_insert()->get_iter().get_line() ==
      Buffer(text_vec_.at(page))->end().get_line()) {
    GdkEventScroll* scroll = new GdkEventScroll;
    scroll->delta_y = 1.0;
    scroll->delta_x = 0.0;
    ScrollEventCallback(scroll);
    delete scroll;
  }
}
void Notebook::Controller
::OnDirectoryNavigation(const Gtk::TreeModel::Path& path,
                        Gtk::TreeViewColumn* column) {
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

Gtk::TextView&  Notebook::Controller::CurrentTextView() {
  return text_vec_.at(CurrentPage())->view();
}

int Notebook::Controller::CurrentPage() {
  return Notebook().get_current_page();
}

Glib::RefPtr<Gtk::TextBuffer>
Notebook::Controller::Buffer(Source::Controller *source) {
  return source->view().get_buffer();
}

int Notebook::Controller::Pages() {
  return Notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::Notebook() {
  return view_.notebook();
}

void Notebook::Controller::TextViewHandlers(Gtk::TextView& textview) {
  textview.get_buffer()->signal_changed().connect(
                                                  [this]() {
                                                    OnBufferChange();
                                                  });

  textview.signal_button_release_event().
    connect(sigc::mem_fun(*this, &Notebook::Controller::OnMouseRelease), false);

  textview.signal_key_release_event().
    connect(sigc::mem_fun(*this, &Notebook::Controller::OnKeyRelease), false);
}

void Notebook::Controller::PopupSelectHandler(Gtk::Dialog &popup,
                                              Gtk::ListViewText &listview,
                                              std::map<std::string, std::string>
                                              *items) {
  listview.signal_row_activated().
    connect([this, &listview, &popup, items](const Gtk::TreeModel::Path& path,
                                             Gtk::TreeViewColumn*) {
              std::string selected = items->
                at(listview.get_text(listview.get_selected()[0]));
              CurrentTextView().get_buffer()->insert_at_cursor(selected);
              popup.response(Gtk::RESPONSE_DELETE_EVENT);
            });
}
void Notebook::Controller::PopupSetSize(Gtk::ScrolledWindow &scroll,
                                        int &current_x,
                                        int &current_y) {
  int textview_x = CurrentTextView().get_width();
  int textview_y = 150;
  bool is_never_scroll_x = true;
  bool is_never_scroll_y = true;
  if (current_x > textview_x) {
    current_x = textview_x;
    is_never_scroll_x = false;
  }
  if (current_y > textview_y) {
    current_y = textview_y;
    is_never_scroll_y = false;
  }
  scroll.set_size_request(current_x, current_y);
  if (!is_never_scroll_x && !is_never_scroll_y) {
    scroll.set_policy(Gtk::PolicyType::POLICY_AUTOMATIC,
                      Gtk::PolicyType::POLICY_AUTOMATIC);
  } else if (!is_never_scroll_x && is_never_scroll_y) {
    scroll.set_policy(Gtk::PolicyType::POLICY_AUTOMATIC,
                      Gtk::PolicyType::POLICY_NEVER);
  } else if (is_never_scroll_x && !is_never_scroll_y) {
    scroll.set_policy(Gtk::PolicyType::POLICY_NEVER,
                      Gtk::PolicyType::POLICY_AUTOMATIC);
  }
}

void Notebook::Controller::FindPopupPosition(Gtk::TextView& textview,
                                             int popup_x,
                                             int popup_y,
                                             int &x,
                                             int &y) {
  Gdk::Rectangle temp1, temp2;
  textview.get_cursor_locations(
                                CurrentTextView().
                                get_buffer()->get_insert()->
                                get_iter(), temp1, temp2);
  int textview_edge_x = 0;
  int textview_edge_y = 0;
  textview.buffer_to_window_coords(
                                   Gtk::TextWindowType::TEXT_WINDOW_WIDGET,
                                   temp1.get_x(),
                                   temp1.get_y(),
                                   x, y);
  Glib::RefPtr<Gdk::Window> gdkw =
    CurrentTextView().get_window(Gtk::TextWindowType::TEXT_WINDOW_WIDGET);
  gdkw->get_origin(textview_edge_x, textview_edge_y);

  x += textview_edge_x;
  y += textview_edge_y;
  if ((textview_edge_x-x)*-1 > textview.get_width()-popup_x) {
    x -= popup_x;
    if (x < textview_edge_x) x = textview_edge_x;
  }
  if ((textview_edge_y-y)*-1 > textview.get_height()-popup_y) {
    y -= (popup_y+14) + 15;
    if (x < textview_edge_y) y = textview_edge_y +15;
  }
}

void Notebook::Controller:: OnSaveFile() {
  if (text_vec_.at(CurrentPage())->is_saved()) {
    std::ofstream file;
    file.open (text_vec_.at(CurrentPage())->path());
    file << CurrentTextView().get_buffer()->get_text();
    file.close();
  } else {
    std::string path = OnSaveFileAs();
    if (path != "") {
      std::ofstream file;
      file.open (path);
      file << CurrentTextView().get_buffer()->get_text();
      file.close();
      text_vec_.at(CurrentPage())->set_file_path(path);
      text_vec_.at(CurrentPage())->set_is_saved(true);
    }
  } 
}


std::string Notebook::Controller::OnSaveFileAs(){
  Gtk::FileChooserDialog dialog("Please choose a file",
				Gtk::FILE_CHOOSER_ACTION_SAVE);
  dialog.set_transient_for(*window_);
  dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Save", Gtk::RESPONSE_OK);
  int result = dialog.run();
   switch (result) {
        case(Gtk::RESPONSE_OK): {
            std::string path = dialog.get_filename();
	    unsigned pos = path.find_last_of("/\\");
	    std::cout << path<< std::endl;
	    //notebook_.OnSaveFile(path);
	    return path;
            break;
        }
        case(Gtk::RESPONSE_CANCEL): {
            break;
        }
        default: {
            std::cout << "Unexpected button clicked." << std::endl;
            break;
        }
    }
   return "";
}
