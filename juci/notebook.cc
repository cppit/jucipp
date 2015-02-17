#include "notebook.h"


Notebook::View::View() :
  view_(Gtk::ORIENTATION_VERTICAL){

}
Gtk::Box& Notebook::View::view() {
  view_.pack_start(notebook_);
  return view_;
}

Notebook::Controller::Controller(Keybindings::Controller& keybindings){
  scrolledwindow_vec_.push_back(new Gtk::ScrolledWindow());
  source_vec_.push_back(new Source::Controller);
  scrolledwindow_vec_.back()->add(source_vec_.back()->view());
  view_.notebook().append_page(*scrolledwindow_vec_.back(), "juCi++");


  keybindings.action_group_menu()->add(Gtk::Action::create("FileMenu",
          Gtk::Stock::FILE));
  /* File->New files */
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNew", "New"));
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewStandard",
                  Gtk::Stock::NEW, "New empty file", "Create a new file"),
          [this]() {
              OnFileNewEmptyfile();
          });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewCC",
                  "New cc file"),
          Gtk::AccelKey("<control><alt>c"),
          [this]() {
              OnFileNewCCFile();
          });
  keybindings.action_group_menu()->add(Gtk::Action::create("FileNewH",
                  "New h file"),
          Gtk::AccelKey("<control><alt>h"),
          [this]() {
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
					  //   OnEditFind();
          });

   entry_.view_.entry().signal_activate().connect(
         [this]() {
   	        OnNewPage(entry_.view_.entry().get_text());
	        entry_.OnHideEntries();
         });
}
  

Gtk::Box& Notebook::Controller::view() {
  return view_.view();
}

Gtk::Box& Notebook::Controller::entry_view(){
  return entry_.view();
}

void Notebook::Controller::OnNewPage(std::string name) {

  scrolledwindow_vec_.push_back(new Gtk::ScrolledWindow());
  source_vec_.push_back(new Source::Controller);
  scrolledwindow_vec_.back()->add(source_vec_.back()->view());
  view_.notebook().append_page(*scrolledwindow_vec_.back(), name);
  view_.notebook().show_all_children();
  view_.notebook().set_focus_child(*scrolledwindow_vec_.back());
  view().show_all_children();
  view_.notebook().set_current_page(view_.notebook().get_n_pages()-1);
}
  
void Notebook::Controller::OnCloseCurrentPage() {
  //TODO (oyvang, zalox, forgi) Save a temp file, in case you close one you dont want to close?
  int page = view_.notebook().get_current_page();
  view_.notebook().remove_page(page);
  delete source_vec_.at(page);
  delete scrolledwindow_vec_.at(page);
  source_vec_.erase(source_vec_.begin()+ page);
  scrolledwindow_vec_.erase(scrolledwindow_vec_.begin()+page);
}

void Notebook::Controller::OnFileNewEmptyfile() {
  entry_.OnShowSetFilenName("");
  //TODO(Oyvang) Legg til funksjon for Entry file name.extension
}

void Notebook::Controller::OnFileNewCCFile() {
  entry_.OnShowSetFilenName(".cc");
  //TODO(Oyvang) Legg til funksjon for Entry file name.extension
}

void Notebook::Controller::OnFileNewHeaderFile() {
  entry_.OnShowSetFilenName(".h");
  //TODO(Oyvang) Legg til funksjon for Entry file name.extension
}




