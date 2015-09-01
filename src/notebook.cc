#include "notebook.h"
#include "logging.h"
#include "sourcefile.h"
#include "singletons.h"
#include <fstream>
#include <regex>
#include "cmake.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
}

Notebook::Notebook(Directories &directories) : Gtk::Notebook(), directories(directories) {
  Gsv::init();
}

int Notebook::size() {
  return get_n_pages();
}

Source::View* Notebook::get_view(int page) {
  return source_views.at(page);
}

Source::View* Notebook::get_current_view() {
  INFO("Getting sourceview");
  return get_view(get_current_page());
}

void Notebook::open(const boost::filesystem::path &file_path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  for(int c=0;c<size();c++) {
    if(file_path==get_view(c)->file_path) {
      set_current_page(c);
      get_current_view()->grab_focus();
      return;
    }
  }
  
  std::ifstream can_read(file_path.string());
  if(!can_read) {
    Singleton::terminal()->print("Error: could not open "+file_path.string()+"\n");
    return;
  }
  can_read.close();
  
  auto language=Source::guess_language(file_path);
  if(language && (language->get_id()=="chdr" || language->get_id()=="c" || language->get_id()=="cpp" || language->get_id()=="objc")) {
    boost::filesystem::path project_path;
    if(directories.cmake && directories.cmake->project_path!="" && file_path.string().substr(0, directories.cmake->project_path.string().size())==directories.cmake->project_path.string())
      project_path=directories.cmake->project_path;
    else {
      project_path=file_path.parent_path();
      CMake cmake(project_path);
      if(cmake.project_path!="") {
        project_path=cmake.project_path;
        Singleton::terminal()->print("Project path for "+file_path.string()+" set to "+project_path.string()+"\n");
      }
      else
        Singleton::terminal()->print("Error: could not find project path for "+file_path.string()+"\n");
    }
    source_views.emplace_back(new Source::ClangView(file_path, project_path, language));
  }
  else
    source_views.emplace_back(new Source::GenericView(file_path, language));
    
  scrolled_windows.emplace_back(new Gtk::ScrolledWindow());
  hboxes.emplace_back(new Gtk::HBox());
  scrolled_windows.back()->add(*source_views.back());
  hboxes.back()->pack_start(*scrolled_windows.back(), true, true);
  
  std::string title=file_path.filename().string();
  append_page(*hboxes.back(), title);
  show_all_children();
  set_current_page(size()-1);
  set_focus_child(*source_views.back());
  get_current_view()->get_buffer()->set_modified(false);
  get_current_view()->grab_focus();
  //Add star on tab label when the page is not saved:
  auto source_view=get_current_view();
  get_current_view()->get_buffer()->signal_modified_changed().connect([this, source_view]() {
    std::string title=source_view->file_path.filename().string();
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
  
  get_current_view()->on_update_status=[this](Source::View* view, const std::string &status) {
    if(get_current_page()!=-1 && get_current_view()==view)
      Singleton::status()->set_text(status);
    else
      Singleton::status()->set_text("");
  };
}

bool Notebook::save(int page) {
  if(page>=size())
    return false;
  auto view=get_view(page);
  if (view->file_path != "" && view->get_buffer()->get_modified()) {
    if(juci::filesystem::write(view->file_path, view->get_buffer())) {
      if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
        for(auto a_view: source_views) {
          if(auto a_clang_view=dynamic_cast<Source::ClangView*>(a_view)) {
            if(clang_view!=a_clang_view)
              a_clang_view->start_reparse_needed=true;
          }
        }
      }
      
      view->get_buffer()->set_modified(false);
      Singleton::terminal()->print("File saved to: " +view->file_path.string()+"\n");
      
      //If CMakeLists.txt have been modified:
      //TODO: recreate cmake even without directories open?
      if(view->file_path.filename()=="CMakeLists.txt") {
        if(directories.cmake && directories.cmake->project_path!="" && view->file_path.string().substr(0, directories.cmake->project_path.string().size())==directories.cmake->project_path.string() && CMake::create_compile_commands(directories.cmake->project_path)) {
          for(auto source_view: source_views) {
            if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view)) {
              if(directories.cmake->project_path.string()==source_clang_view->project_path) {
                if(source_clang_view->restart_parse())
                  Singleton::terminal()->async_print("Reparsing "+source_clang_view->file_path.string()+"\n");
                else
                  Singleton::terminal()->async_print("Already reparsing "+source_clang_view->file_path.string()+". Please reopen the file manually.\n");
              }
            }
          }
        }
      }
      
      return true;
    }
    Singleton::terminal()->print("Error: could not save file " +view->file_path.string()+"\n");
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
  DEBUG("start");
  INFO("Notebook close page");
  if (get_current_page()!=-1) {
    if(get_current_view()->get_buffer()->get_modified()){
      if(!save_modified_dialog()) {
        DEBUG("end false");
        return false;
      }
    }
    int page = get_current_page();
    remove_page(page);
    if(get_current_page()==-1)
      Singleton::status()->set_text("");
    auto source_view=source_views.at(page);
    if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view))
      source_clang_view->async_delete();
    else
      delete source_view;
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
  }
  DEBUG("end true");
  return true;
}

bool Notebook::save_modified_dialog() {
  INFO("Notebook::save_modified_dialog");
  Gtk::MessageDialog dialog((Gtk::Window&)(*get_toplevel()), "Save file!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_secondary_text("Do you want to save: " + get_current_view()->file_path.string()+" ?");
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

