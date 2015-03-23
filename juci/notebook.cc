
#include "notebook.h"

Notebook::Model::Model() {
  cc_extension_ = ".cc";
  h_extension_  = ".h";
  scrollvalue_ = 20;
}
Notebook::View::View(){
  view_.pack_start(notebook_);  
}

Notebook::Controller::Controller(Keybindings::Controller& keybindings,
				 Source::Config& source_cfg) :
source_config_(source_cfg) { 
  OnNewPage("juCi++");
  refClipboard_ = Gtk::Clipboard::get();
  keybindings.action_group_menu()->add(Gtk::Action::create("FileMenu",
                                                           Gtk::Stock::FILE));
  /* File->New files */

  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewStandard",
                                                           Gtk::Stock::NEW,
                                                           "New empty file",
                                                           "Create a new file"),
                                       [this]() {
					 is_new_file_ = true;
                                         OnFileNewEmptyfile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewCC",
                                                           "New cc file"),
                                       Gtk::AccelKey(keybindings.config_
						     .key_map()["new_cc_file"]),
                                       [this]() {
					 is_new_file_ = true;
                                         OnFileNewCCFile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewH",
                                                           "New h file"),
                                       Gtk::AccelKey(keybindings.config_
						     .key_map()["new_h_file"]),
                                       [this]() {
					 is_new_file_ = true;
                                         OnFileNewHeaderFile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("WindowCloseTab",
                                                           "Close tab"),
                                       Gtk::AccelKey(keybindings.config_
						     .key_map()["close_tab"]),
                                       [this]() {
                                         OnCloseCurrentPage();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("EditFind",
                                                           Gtk::Stock::FIND),
                                       [this]() {
					 is_new_file_ = false;
					 OnEditSearch();
		       //TODO(Oyvang, Zalox, Forgi)Create function OnEditFind();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("EditCopy",
                                                           Gtk::Stock::COPY),  
                                       [this]() {
                                         OnEditCopy();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("EditCut",
                                                           Gtk::Stock::CUT),
                                       [this]() {
                                         OnEditCut();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("EditPaste",
                                                           Gtk::Stock::PASTE),
                                       [this]() {
                                         OnEditPaste();
                                       });
  entry_.view_.entry().signal_activate().connect(
                                                 [this]() {
						   if(is_new_file_){
						     OnNewPage(entry_.text());
						     entry_.OnHideEntries(is_new_file_);
						   }else{
						     Search(true);
						   }
                                                 });
  entry_.button_apply().signal_clicked().connect(
						 [this]() {   
						   OnNewPage(entry_.text());
						   entry_.OnHideEntries(is_new_file_);  
						 });
  entry_.button_close().signal_clicked().connect(
						 [this]() {   
						   entry_.OnHideEntries(is_new_file_);  
						 });
  entry_.button_next().signal_clicked().connect(
						[this]() {   
						  Search(true); 
						});
  entry_.button_prev().signal_clicked().connect(
						[this]() {   
						  Search(false); 
						});

  text_vec_.back()->view().
    signal_scroll_event().connect(sigc::mem_fun(
						this,
						&Notebook::Controller::
						scroll_event_callback));

  
  
}//Constructor


bool Notebook::Controller::scroll_event_callback(GdkEventScroll* scroll_event) {
  int page = CurrentPage();
  int direction_y = scroll_event->delta_y;
  int direction_x = scroll_event->delta_x;

  Glib::RefPtr<Gtk::Adjustment> adj =
    scrolledtext_vec_.at(page)->
    get_vscrollbar()->get_adjustment();
  
  if ( direction_y != 0 ) {
    int dir_val = direction_y==-1?-model_.scrollvalue_:+model_.scrollvalue_; 
    adj->set_value(adj->get_value()+dir_val);
    text_vec_.at(page)->view().set_vadjustment(adj);
    linenumbers_vec_.at(page)->view().set_vadjustment(adj);
  }
  if ( direction_x != 0 ) {
    int dir_val = direction_x==-1?-model_.scrollvalue_:+model_.scrollvalue_; 
    adj->set_value(adj->get_value()+dir_val);
    text_vec_.at(page)->view().set_hadjustment(adj);
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

Gtk::HBox& Notebook::Controller::view() {
  return view_.view();
}
Gtk::Box& Notebook::Controller::entry_view() {
  return entry_.view();
}


void Notebook::Controller::OnNewPage(std::string name) {
  OnCreatePage();
  std::cout << "oppretta pages" << std::endl;
  text_vec_.back()->OnNewEmptyFile();
  Notebook().append_page(*editor_vec_.back(), name);
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  Notebook().set_focus_child(text_vec_.at(Pages()-1)->view()); 
}

void Notebook::Controller::OnOpenFile(std::string path) {
  OnCreatePage();
  text_vec_.back()->OnOpenFile(path);
  unsigned pos = path.find_last_of("/\\");
  Notebook().append_page(*editor_vec_.back(), path.substr(pos+1));
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  Notebook().set_focus_child(text_vec_.back()->view());
  OnBufferChange();
}

void Notebook::Controller::OnCreatePage(){
  text_vec_.push_back(new Source::Controller(source_config())); // add arguments
  linenumbers_vec_.push_back(new Source::Controller(source_config())); // add arguments
  scrolledline_vec_.push_back(new Gtk::ScrolledWindow());
  scrolledtext_vec_.push_back(new Gtk::ScrolledWindow());
  editor_vec_.push_back(new Gtk::HBox());
  scrolledtext_vec_.back()->add(text_vec_.back()->view());
  scrolledline_vec_.back()->add(linenumbers_vec_.back()->view());
  linenumbers_vec_.back()->view().get_buffer()->set_text("1 \n");
  linenumbers_vec_.back()->view().override_color(Gdk::RGBA("Black"));
  linenumbers_vec_.back()->
    view().set_justification(Gtk::Justification::JUSTIFY_RIGHT);
  scrolledline_vec_.back()->get_vscrollbar()->hide();
  linenumbers_vec_.back()->view().set_editable(false);
  linenumbers_vec_.back()->view().set_sensitive(false);
  editor_vec_.back()->pack_start(*scrolledline_vec_.back(),false,false);
  editor_vec_.back()->pack_start(*scrolledtext_vec_.back(), true, true);
  BufferChangeHandler(text_vec_.back()->view().get_buffer());
}

void Notebook::Controller::OnCloseCurrentPage() {
  //TODO (oyvang, zalox, forgi) Save a temp file, in case you close one you dont want to close?
  if(Pages()!=0){
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

std::string Notebook::Controller::GetCursorWord(){
  int page = CurrentPage();
  std::string word;
  Gtk::TextIter start,end;
  start = Buffer(text_vec_.at(page))->get_insert()->get_iter();
  end = Buffer(text_vec_.at(page))->get_insert()->get_iter();
  if(!end.ends_line()) {
    while(!end.ends_word()){
      end.forward_char();
    }
  }
  if(!start.starts_line()) {
    while(!start.starts_word()){
      start.backward_char();
    }
  }
  word = Buffer(text_vec_.at(page))->get_text(start,end);
  //TODO(Oyvang)fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch() {
  search_match_end_ =
    Buffer(text_vec_.at(CurrentPage()))->get_iter_at_offset(0);
  entry_.OnShowSearch(GetCursorWord());
}

void Notebook::Controller::Search(bool forward){
  int page = CurrentPage();
  std::string search_word;
  search_word = entry_.text();
  Gtk::TextIter test;

  if ( !forward ) {
    if ( search_match_start_ == 0 ||
       search_match_start_.get_line_offset() == 0) {
      search_match_start_= Buffer(text_vec_.at(CurrentPage()))->end();
    }
    search_match_start_.
      backward_search(search_word,
		      Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
		      Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
		      search_match_start_,
		      search_match_end_);  
  } else {
    if ( search_match_end_ == 0 ) {
      search_match_end_= Buffer(text_vec_.at(CurrentPage()))->begin();
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
   int line_nr = Buffer(text_vec_.at(page))->get_line_count();

   Glib::RefPtr
     <Gtk::TextBuffer::Mark> mark = Gtk::TextBuffer::Mark::create();
   Glib::RefPtr
     <Gtk::TextBuffer::Mark> mark_lines = Gtk::TextBuffer::Mark::create();

   if(Buffer(text_vec_.at(page))->get_insert()->get_iter().starts_line() &&
      Buffer(text_vec_.at(page))->get_insert()->get_iter().get_line() ==
      Buffer(text_vec_.at(page))->end().get_line()) {
     std::string lines ="1 ";
     for ( int it = 2; it <= line_nr; ++it ) {
       lines.append("\n"+ std::to_string(it)+" ");
     }
     Buffer(linenumbers_vec_.at(page))->set_text(lines);
     
     Buffer(text_vec_.at(page))->
       add_mark(
		mark,
		Buffer(text_vec_.at(page))->end());
     Buffer(linenumbers_vec_.at(page))->
       add_mark(
		mark_lines,
		Buffer(linenumbers_vec_.at(page))->end());

     text_vec_.at(page)->view().scroll_to(mark); 
     linenumbers_vec_.at(page)->view().scroll_to(mark_lines);
   }else{
     Buffer(text_vec_.at(page))->
       add_mark(
		mark,
	        Buffer(text_vec_.at(page))->
		get_insert()->get_iter());
   }
}


Gtk::TextView&  Notebook::Controller::CurrentTextView() {
  return text_vec_.at(CurrentPage())->view();
}

int Notebook::Controller::CurrentPage() {
  return Notebook().get_current_page();
}

Glib::RefPtr<Gtk::TextBuffer>
Notebook::Controller::Buffer( Source::Controller *source ) {
  return source->view().get_buffer();
}

int Notebook::Controller::Pages() {
  return Notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::Notebook() {
  return view_.notebook();
}

void Notebook::Controller::BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer>
					       buffer) {
  buffer->signal_changed().connect(
				   [this]() {   
				     OnBufferChange(); 
				   });
}
