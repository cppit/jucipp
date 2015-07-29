#include <fstream>
#include "notebook.h"
#include "logging.h"
#include "singletons.h"
#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Notebook::Notebook() : Gtk::Notebook() {
  Gsv::init();
}

int Notebook::size() {
  return get_n_pages();
}

Source::View* Notebook::get_view(int page) {
  if(page>=size())
    return nullptr;
  return source_views.at(page).get();  
}

Source::View* Notebook::get_current_view() {
  INFO("Getting sourceview");
  if(get_current_page()==-1)
    return nullptr;
  return get_view(get_current_page());
}

void Notebook::open(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  for(int c=0;c<size();c++) {
    if(path==get_view(c)->file_path) {
      set_current_page(c);
      return;
    }
  }
  
  auto tmp_project_path=project_path;
  if(tmp_project_path=="") {
    tmp_project_path=boost::filesystem::path(path).parent_path().string();
  }
  auto language=Source::guess_language(path);
  if(language && (language->get_id()=="chdr" || language->get_id()=="c" || language->get_id()=="cpp" || language->get_id()=="objc"))
    source_views.emplace_back(new Source::ClangView(path, tmp_project_path));
  else
    source_views.emplace_back(new Source::GenericView(path, tmp_project_path, language));
    
  scrolled_windows.emplace_back(new Gtk::ScrolledWindow());
  hboxes.emplace_back(new Gtk::HBox());
  scrolled_windows.back()->add(*source_views.back());
  hboxes.back()->pack_start(*scrolled_windows.back(), true, true);
  
  boost::filesystem::path file_path(source_views.back()->file_path);
  std::string title=file_path.filename().string();
  append_page(*hboxes.back(), title);
  show_all_children();
  set_current_page(size()-1);
  set_focus_child(*source_views.back());
  get_current_view()->get_buffer()->set_modified(false);
  //Add star on tab label when the page is not saved:
  auto source_view=get_current_view();
  get_current_view()->get_buffer()->signal_modified_changed().connect([this, source_view]() {
    boost::filesystem::path file_path(source_view->file_path);
    std::string title=file_path.filename().string();
    if(source_view->get_buffer()->get_modified())
      title+="*";
    int page=-1;
    for(int c=0;c<size();c++) {
      if(get_view(c)==source_view) {
        page=c;
        break;
      }
    }
    if(page!=-1)
      set_tab_label_text(*(get_nth_page(page)), title);
  });
}

bool Notebook::save(int page) {
  if(page>=size())
    return false;
  auto view=get_view(page);
  if (view->file_path != "" && view->get_buffer()->get_modified()) {
    std::ofstream file;
    file.open(view->file_path);
    file << view->get_buffer()->get_text();
    file.close();
    view->get_buffer()->set_modified(false);
    Singleton::terminal()->print("File saved to: " +view->file_path+"\n");
    return true;
  }
  return false;
}

bool Notebook::save_current() {
  INFO("Notebook save current file");
  if(get_current_page()==-1)
    return false;
  return save(get_current_page());
}

bool Notebook::close_current_page() {
  INFO("Notebook close page");
  if (size() != 0) {
    if(get_current_view()->get_buffer()->get_modified()){
      if(!save_modified_dialog())
        return false;
    }
    int page = get_current_page();
    remove_page(page);
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
  }
  return true;
}

bool Notebook::save_modified_dialog() {
  INFO("Notebook::save_modified_dialog");
  Gtk::MessageDialog dialog((Gtk::Window&)(*get_toplevel()), "Save file!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_secondary_text("Do you want to save: " + get_current_view()->file_path+" ?");
  int result = dialog.run();
  if(result==Gtk::RESPONSE_YES) {
    save_current();
    return true;
  }
  else if(result==Gtk::RESPONSE_NO) {
    return true;
  }
  else {
    return false;
  }
}

