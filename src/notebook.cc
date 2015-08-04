#include "notebook.h"
#include "logging.h"
#include "sourcefile.h"
#include "singletons.h"
#include <fstream>
#include <regex>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Notebook::Notebook(Directories &directories) : Gtk::Notebook(), directories(directories) {
  Gsv::init();
}

int Notebook::size() {
  return get_n_pages();
}

Source::View* Notebook::get_view(int page) {
  if(page>=size())
    return nullptr;
  return source_views.at(page);
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
      get_current_view()->grab_focus();
      return;
    }
  }
  
  std::ifstream can_read(path);
  if(!can_read) {
    Singleton::terminal()->print("Error: could not open "+path+"\n");
    return;
  }
  can_read.close();
  
  auto language=Source::guess_language(path);
  if(language && (language->get_id()=="chdr" || language->get_id()=="c" || language->get_id()=="cpp" || language->get_id()=="objc")) {
    auto view_project_path=project_path;
    if(view_project_path=="") {
      view_project_path=boost::filesystem::path(path).parent_path().string();
      auto found_project_path=find_project_path(view_project_path);
      if(found_project_path!="") {
        view_project_path=found_project_path;
        Singleton::terminal()->print("Project path for "+path+" set to "+view_project_path+"\n");
      }
      else
        Singleton::terminal()->print("Error: could not find project path for "+path+"\n");
    }
    if(boost::filesystem::exists(view_project_path+"/CMakeLists.txt") && !boost::filesystem::exists(view_project_path+"/compile_commands.json"))
      make_compile_commands(view_project_path);
    source_views.emplace_back(new Source::ClangView(path, view_project_path));
  }
  else {
    auto view_project_path=project_path;
    if(view_project_path=="")
      view_project_path=boost::filesystem::path(path).parent_path().string();
    source_views.emplace_back(new Source::GenericView(path, view_project_path, language));
  }
    
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
  get_current_view()->grab_focus();
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
  
  get_current_view()->on_update_status=[this](Source::View* view, const std::string &status) {
    if(get_current_view()==view)
      Singleton::status()->set_text(status);
  };
}

std::string Notebook::find_project_path(const std::string &path) {
  const auto find_cmake_project=[this](const boost::filesystem::path &path) {
    auto cmake_path=path;
    cmake_path+="/CMakeLists.txt";
    for(auto &line: juci::filesystem::read_lines(cmake_path)) {
      const std::regex cmake_project("^ *project *\\(.*$");
      std::smatch sm;
      if(std::regex_match(line, sm, cmake_project)) {
        return true;
      }
    }
    return false;
  };
  
  auto boost_path=boost::filesystem::path(path);
  if(find_cmake_project(boost_path))
    return boost_path.string();
  do {
    boost_path=boost_path.parent_path();
    if(find_cmake_project(boost_path))
      return boost_path.string();
  } while(boost_path!=boost_path.root_directory());

  return "";
}

bool Notebook::save(int page) {
  if(page>=size())
    return false;
  auto view=get_view(page);
  if (view->file_path != "" && view->get_buffer()->get_modified()) {
    if(juci::filesystem::write(view->file_path, view->get_buffer())) {
      view->get_buffer()->set_modified(false);
      Singleton::terminal()->print("File saved to: " +view->file_path+"\n");
      
      //If CMakeLists.txt have been modified:
      if(boost::filesystem::path(view->file_path).filename().string()=="CMakeLists.txt") {
        if(project_path!="" && make_compile_commands(project_path)) {
          for(auto source_view: source_views) {
            if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view)) {
              if(project_path==source_view->project_path) {
                if(source_clang_view->restart_parse())
                  Singleton::terminal()->print("Reparsing "+source_clang_view->file_path+"\n");
                else
                  Singleton::terminal()->print("Already reparsing "+source_clang_view->file_path+". Please reopen the file manually.\n");
              }
            }
          }
        }
      }
      
      return true;
    }
    Singleton::terminal()->print("Error: could not save file " +view->file_path+"\n");
  }
  return false;
}

bool Notebook::make_compile_commands(const std::string &path) {
  Singleton::terminal()->print("Creating "+boost::filesystem::path(path+"/compile_commands.json").string()+"\n");
  //TODO: Windows...
  if(Singleton::terminal()->execute(path, "cmake . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>&1")) {
    if(project_path!="")
      directories.open_folder(project_path);
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
    if(get_current_page()==-1)
      Singleton::status()->set_text("");
    auto source_view=source_views.at(page);
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
    if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view))
      source_clang_view->async_delete();
    else
      delete source_view;
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

