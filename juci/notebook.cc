#include "notebook.h"

Notebook::Model::Model() {
  cc_extension = ".cc";
  h_extension  = ".h";
};
Notebook::View::View() :
  view_(Gtk::ORIENTATION_VERTICAL){

  
}
Gtk::Box& Notebook::View::view() {
  view_.pack_start(notebook_);
  return view_;
}
Notebook::Controller::Controller(Keybindings::Controller& keybindings){

  OnNewPage("juCi++");
  refClipboard = Gtk::Clipboard::get();
  
  keybindings.action_group_menu()->add(Gtk::Action::create("FileMenu",
                                                           Gtk::Stock::FILE));
  /* File->New files */

  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewStandard",
                                                           Gtk::Stock::NEW,
                                                           "New empty file",
                                                           "Create a new file"),
                                       [this]() {
					 is_new_file = true;
                                         OnFileNewEmptyfile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewCC",
                                                           "New cc file"),
                                       Gtk::AccelKey("<control><alt>c"),
                                       [this]() {
					 is_new_file = true;
                                         OnFileNewCCFile();
                                       });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewH",
                                                           "New h file"),
                                       Gtk::AccelKey("<control><alt>h"),
                                       [this]() {
					 is_new_file = true;
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
					 is_new_file = false;
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
						   if(is_new_file){
						     OnNewPage(entry_.text());
						     entry_.OnHideEntries(is_new_file);
						   }else{
						     Search(true);
						   }
                                                 });
  entry_.button_apply().signal_clicked().connect(
						 [this]() {   
						   OnNewPage(entry_.text());
						   entry_.OnHideEntries(is_new_file);  
						 });
  entry_.button_close().signal_clicked().connect(
						 [this]() {   
						   entry_.OnHideEntries(is_new_file);  
						 });
  entry_.button_next().signal_clicked().connect(
						[this]() {   
						  Search(true); 
						});
  entry_.button_prev().signal_clicked().connect(
						[this]() {   
						  Search(false); 
						});

  
  
}//Constructor
Gtk::Box& Notebook::Controller::view() {
  return view_.view();
}
Gtk::Box& Notebook::Controller::entry_view(){
  return entry_.view();
}
void Notebook::Controller::OnNewPage(std::string name) {
  scrolledwindow_vec_.push_back(new Gtk::ScrolledWindow());
  source_vec_.push_back(new Source::Controller);
  label_vec_.push_back(new Gtk::Label);
  label_vec_.back()->set_text("1\n");
  label_vec_.back()->set_justify(Gtk::Justification::JUSTIFY_RIGHT);
  label_vec_.back()->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_START);
  paned_vec_.push_back(new Gtk::Paned());
  paned_vec_.back()->set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
  paned_vec_.back()->add1(*label_vec_.back());
  paned_vec_.back()->pack2(source_vec_.back()->view(),true,false);
  scrolledwindow_vec_.back()->add(*paned_vec_.back());
  source_vec_.back()->OnNewEmptyFile();
    scrolledwindow_vec_.back()->set_policy(Gtk::PolicyType::POLICY_NEVER,
					 Gtk::PolicyType::POLICY_AUTOMATIC);
  notebook().append_page(*scrolledwindow_vec_.back(), name);
  notebook().show_all_children();
  notebook().set_focus_child(*scrolledwindow_vec_.back());
  notebook().set_current_page(pages()-1);
}
void Notebook::Controller::OnCloseCurrentPage() {
  //TODO (oyvang, zalox, forgi) Save a temp file, in case you close one you dont want to close?
  int page = currentPage();
  notebook().remove_page(page);
  delete source_vec_.at(page);
  delete scrolledwindow_vec_.at(page);
  source_vec_.erase(source_vec_.begin()+ page);
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
  if (pages() != 0) {
    buffer()->copy_clipboard(refClipboard);
  }
}
void Notebook::Controller::OnEditPaste() {
  if (pages() != 0) {
    buffer()->paste_clipboard(refClipboard);
  }
}
void Notebook::Controller::OnEditCut() {
  if (pages() != 0) {
    buffer()->cut_clipboard(refClipboard);
  }
}

void Notebook::Controller::OnOpenFile(std::string path) {
  scrolledwindow_vec_.push_back(new Gtk::ScrolledWindow());
  source_vec_.push_back(new Source::Controller);
  scrolledwindow_vec_.back()->add(source_vec_.back()->view());
  source_vec_.back()->OnOpenFile(path);
  scrolledwindow_vec_.back()->set_policy(Gtk::PolicyType::POLICY_NEVER,
					 Gtk::PolicyType::POLICY_AUTOMATIC);
  unsigned pos = path.find_last_of("/\\");
  notebook().append_page(*scrolledwindow_vec_.back(), path.substr(pos+1));
  notebook().show_all_children();
  notebook().set_focus_child(*scrolledwindow_vec_.back());
  notebook().set_current_page(pages()-1);
}

std::string Notebook::Controller::GetCursorWord(){
  std::string word;
  Gtk::TextIter start,end;
  start = buffer()->get_insert()->get_iter();
  end = buffer()->get_insert()->get_iter();
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
  word = buffer()->get_text(start,end);

  //TODO(Oyvang)fix selected text
  return word;
}

void Notebook::Controller::OnEditSearch(){
  search_match_end_ = buffer()->get_iter_at_offset(0);
  entry_.OnShowSearch(GetCursorWord());   
   
}

void Notebook::Controller::Search(bool forward){
  std::string search_word;
  search_word = entry_.text();
  Gtk::TextIter test;

  if(!forward){
    if(search_match_start_ == 0 || search_match_start_.get_line_offset() == 0) {
      search_match_start_= buffer()->end();
    }
    search_match_start_.backward_search(search_word,
					Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY|
					Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
					search_match_start_, search_match_end_);  
  }else{
    if(search_match_end_ == 0) {
      search_match_end_= buffer()->begin();
    }
    search_match_end_.forward_search(search_word,
				     Gtk::TextSearchFlags::TEXT_SEARCH_TEXT_ONLY |
				     Gtk::TextSearchFlags::TEXT_SEARCH_VISIBLE_ONLY,
				     search_match_start_, search_match_end_);
  }

  // std::cout << "matc_start - "
  //   << search_match_start_.get_line_offset()
  //   //<< test.get_line_offset()
  // 	    << "  ||  match_end - "
  // 	    << search_match_end_.get_line_offset()
  // 		<< std::endl;


}


int Notebook::Controller::currentPage(){
  return notebook().get_current_page();
}

Glib::RefPtr<Gtk::TextBuffer> Notebook::Controller::buffer(){
  return source_vec_.at(currentPage())->view().get_buffer();
}

int Notebook::Controller::pages(){
  return notebook().get_n_pages();
}
Gtk::Notebook& Notebook::Controller::notebook(){
  return view_.notebook();
}
