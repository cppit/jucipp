
#include "notebook.h"

Notebook::Model::Model() {
  cc_extension = ".cc";
  h_extension  = ".h";
};
Notebook::View::View(){
  linenumbers_.set_margin_top(30);
  linenumbers_.set_justify(Gtk::Justification::JUSTIFY_RIGHT);
  linenumbers_.set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_START);
  //line.add(linenumbers_);
  view_.pack_start(notebook_);  
}

Notebook::Controller::Controller(Keybindings::Controller& keybindings) {
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
                                       Gtk::AccelKey("<control><alt>c"),
                                       [this]() {
					 is_new_file_ = true;
                                         OnFileNewCCFile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewH",
                                                           "New h file"),
                                       Gtk::AccelKey("<control><alt>h"),
                                       [this]() {
					 is_new_file_ = true;
                                         OnFileNewHeaderFile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("WindowCloseTab",
                                                           "Close tab"),
                                       Gtk::AccelKey("<control>w"),
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

  source_vec_.back()->view().
    signal_scroll_event().connect(
				  sigc::mem_fun(
						this,
						&Notebook::Controller::
						scroll_event_callback));

  
  
}//Constructor

  
bool Notebook::Controller::scroll_event_callback(GdkEventScroll* scroll_event) {
  
  Glib::RefPtr<Gtk::Adjustment> adj =
    scrolledeverything_vec_.back()->
    get_vscrollbar()->get_adjustment();

  if(scroll_event->delta_y == -1) {
     adj->set_value(adj->get_value()-10);
   } else {
     adj->set_value(adj->get_value()+10);
   }

   source_vec_.back()->view().set_vadjustment(adj);
   linenumbers.back()->view().set_vadjustment(adj);
 

   return true;
 }

Notebook::Controller::~Controller() {
  for (auto &i : source_vec_) delete i;
  for (auto &i : label_vec_) delete i;
  for (auto &i : box_h) delete i;
  for (auto &i : box_l) delete i;
  for (auto &i : box_m) delete i;
  for (auto &i : scrolledeverything_vec_) delete i;
  for (auto &i : scrolledwindow_vec_) delete i;
}

Gtk::HBox& Notebook::Controller::view() {
  return view_.view();
}
Gtk::Box& Notebook::Controller::entry_view() {
  return entry_.view();y
}


void Notebook::Controller::OnNewPage(std::string name) {
  std::cout << "new page" << std::endl;y
  OnCreatePage();
  std::cout << "oppretta pages" << std::endl;
  source_vec_.back()->OnNewEmptyFile();
  Notebook().append_page(*box_m.back(), name);
  Notebook().show_all_children();
  Notebook().set_current_page(Pages()-1);
  BufferChangeHandler(source_vec_.back()->view().get_buffer());
}

void Notebook::Controller::OnOpenFile(std::string path) {
  OnCreatePage();
  source_vec_.back()->OnOpenFile(path);
  unsigned pos = path.find_last_of("/\\");
  Notebook().append_page(*scrolledwindow_vec_.back(), path.substr(pos+1));
  Notebook().show_all_children();
  Notebook().set_focus_child(*scrolledwindow_vec_.back());
  Notebook().set_current_page(Pages()-1);
   BufferChangeHandler(source_vec_.back()->view().get_buffer());
  OnBufferChange();
}

void Notebook::Controller::OnCreatePage(){
  scrolledwindow_vec_.push_back(new Gtk::ScrolledWindow());
  source_vec_.push_back(new Source::Controller);
  linenumbers.push_back(new Source::Controller);
  scrolledeverything_vec_.push_back(new Gtk::ScrolledWindow());
  label_vec_.push_back(new Gtk::Label());
  // box_l.push_back(new Gtk::HBox());
  //box_h.push_back(new Gtk::HBox());
  box_m.push_back(new Gtk::HBox());
  scrolledeverything_vec_.back()->add(source_vec_.back()->view());
  linesscroll.add(linenumbers.back()->view());
  linenumbers.back()->view().get_buffer()->set_text("1\n");

  linesscroll.get_vscrollbar()->hide();

  linenumbers.back()->view().set_editable(false);
  linenumbers.back()->view().set_sensitive(false);
  box_m.back()->pack_start(linesscroll,false,false);
  box_m.back()->pack_start(*scrolledeverything_vec_.back(), true, true);

}

void Notebook::Controller::OnCloseCurrentPage() {
  //TODO (oyvang, zalox, forgi) Save a temp file, in case you close one you dont want to close?
  int page = CurrentPage();
  Notebook().remove_page(page);
  delete source_vec_.at(page);
  delete label_vec_.at(page);
  delete box_h.at(page);
  delete box_l.at(page);
  delete box_m.at(page);
  delete scrolledeverything_vec_.at(page);
  delete scrolledwindow_vec_.at(page);

  source_vec_.erase(source_vec_.begin()+ page);
  label_vec_.erase(label_vec_.begin()+page);
  box_h.erase(box_h.begin()+page);
  box_l.erase(box_l.begin()+page);
  box_m.erase(box_m.begin()+page);
  scrolledeverything_vec_.erase(scrolledeverything_vec_.begin()+page);
  scrolledwindow_vec_.erase(scrolledwindow_vec_.begin()+page);

}
void Notebook::Controller::OnFileNewEmptyfile() {
  entry_.OnShowSetFilenName("");
}
void Notebook::Controller::OnFileNewCCFile() {
  entry_.OnShowSetFilenName(model_.cc_extension);
}
void Notebook::Controller::OnFileNewHeaderFile() {
  entry_.OnShowSetFilenName(model_.h_extension);
}
void Notebook::Controller::OnEditCopy() {
  if (Pages() != 0) {
    Buffer()->copy_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditPaste() {
  if (Pages() != 0) {
    Buffer()->paste_clipboard(refClipboard_);
  }
}
void Notebook::Controller::OnEditCut() {
  if (Pages() != 0) {
    Buffer()->cut_clipboard(refClipboard_);
  }
}

std::string Notebook::Controller::GetCursorWord(){
  std::string word;
  Gtk::TextIter start,end;
  start = Buffer()->get_insert()->get_iter();
  end = Buffer()->get_insert()->get_iter();
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
  word = Buffer()->get_text(start,end);
  //TODO(Oyvang)fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch() {
  search_match_end_ = Buffer()->get_iter_at_offset(0);
  entry_.OnShowSearch(GetCursorWord());   
}

void Notebook::Controller::Search(bool forward){
  std::string search_word;
  search_word = entry_.text();
  Gtk::TextIter test;

  if(!forward){
    if(search_match_start_ == 0 || search_match_start_.get_line_offset() == 0) {
      search_match_start_= Buffer()->end();
    }
    search_match_start_.backward_search(search_word,
					Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY|
					Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
					search_match_start_, search_match_end_);  
  }else{
    if(search_match_end_ == 0) {
      search_match_end_= Buffer()->begin();
    }
    search_match_end_.forward_search(search_word,
				     Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
				     Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
				     search_match_start_, search_match_end_);
  }
}

void Notebook::Controller::OnBufferChange() {
   int page =  CurrentPage();
   int line_nr = Buffer()->get_line_count();


  


 Glib::RefPtr<Gtk::TextBuffer::Mark> mark = Gtk::TextBuffer::Mark::create();
 Glib::RefPtr<Gtk::TextBuffer::Mark> mark_lines = Gtk::TextBuffer::Mark::create();

 if(source_vec_.at(page)->view().get_buffer()->get_insert()->get_iter().starts_line() &&
    source_vec_.at(page)->view().get_buffer()->get_insert()->get_iter().get_line() == Buffer()->end().get_line()){


    std::string lines ="1";
 for (int it = 2; it <= line_nr; ++it) {
   lines.append("\n"+ std::to_string(it)+"");

} 
 linenumbers.back()->view().get_buffer()->set_text(lines);


 source_vec_.at(page)->view().get_buffer()->add_mark(mark, Buffer()->end());
 linenumbers.at(page)->view().get_buffer()->add_mark(mark_lines,linenumbers.at(page)->view().get_buffer()->end());

 source_vec_.at(page)->view().scroll_to(mark); 
 linenumbers.at(page)->view().scroll_to(mark_lines);
 }else{
    source_vec_.at(page)->view().get_buffer()->add_mark(mark, Buffer()->get_insert()->get_iter());
 }
}


int Notebook::Controller::CurrentPage() {
  return Notebook().get_current_page();
}

Glib::RefPtr<Gtk::TextBuffer> Notebook::Controller::Buffer() {
  return source_vec_.at(CurrentPage())->view().get_buffer();
}

int Notebook::Controller::Pages() {
  return Notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::Notebook() {
  return view_.notebook();
}
Gtk::Label& Notebook::Controller::Linenumbers() {
  return view_.linenumbers();
}

void Notebook::Controller::BufferChangeHandler(Glib::RefPtr<Gtk::TextBuffer>
					       buffer) {
  buffer->signal_changed().connect(
				   [this]() {   
				     OnBufferChange(); 
				   });
}
