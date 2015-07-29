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
  INFO("Create notebook");
  Gsv::init();

  auto menu=Singleton::menu();
  INFO("Notebook create signal handlers");
  menu->action_group->add(Gtk::Action::create("FileMenu", "File"));

  menu->action_group->add(Gtk::Action::create("WindowCloseTab", "Close tab"), Gtk::AccelKey(menu->key_map["close_tab"]), [this]() {
    close_current_page();
  });

  menu->action_group->add(Gtk::Action::create("EditUndo", "Undo"), Gtk::AccelKey(menu->key_map["edit_undo"]), [this]() {
	  INFO("On undo");
    Glib::RefPtr<Gsv::UndoManager> undo_manager = CurrentSourceView()->get_source_buffer()->get_undo_manager();
    if (Pages() != 0 && undo_manager->can_undo()) {
      undo_manager->undo();
    }
    INFO("Done undo");
	});

  menu->action_group->add(Gtk::Action::create("EditRedo", "Redo"), Gtk::AccelKey(menu->key_map["edit_redo"]), [this]() {
    INFO("On Redo");
    Glib::RefPtr<Gsv::UndoManager> undo_manager =
    CurrentSourceView()->get_source_buffer()->get_undo_manager();
    if (Pages() != 0 && undo_manager->can_redo()) {
      undo_manager->redo();
    }
    INFO("Done Redo");
  });
  
  menu->action_group->add(Gtk::Action::create("SourceGotoDeclaration", "Go to declaration"), Gtk::AccelKey(menu->key_map["source_goto_declaration"]), [this]() {
    if(CurrentPage()!=-1) {
      if(CurrentSourceView()->get_declaration_location) {
        auto location=CurrentSourceView()->get_declaration_location();
        if(location.first.size()>0) {
          open_file(location.first);
          CurrentSourceView()->get_buffer()->place_cursor(CurrentSourceView()->get_buffer()->get_iter_at_offset(location.second));
          while(gtk_events_pending())
            gtk_main_iteration();
          CurrentSourceView()->scroll_to(CurrentSourceView()->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        }
      }
    }
  });
  
  menu->action_group->add(Gtk::Action::create("SourceGotoMethod", "Go to method"), Gtk::AccelKey(menu->key_map["source_goto_method"]), [this]() {
    if(CurrentPage()!=-1) {
      if(CurrentSourceView()->goto_method) {
        CurrentSourceView()->goto_method();
      }
    }
  });
}

void Notebook::open_file(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  for(int c=0;c<Pages();c++) {
    if(path==source_views.at(c)->view->file_path) {
      set_current_page(c);
      return;
    }
  }
  source_views.emplace_back(new Source(path, project_path));
  scrolled_windows.emplace_back(new Gtk::ScrolledWindow());
  hboxes.emplace_back(new Gtk::HBox());
  scrolled_windows.back()->add(*source_views.back()->view);
  hboxes.back()->pack_start(*scrolled_windows.back(), true, true);
  
  boost::filesystem::path file_path(source_views.back()->view->file_path);
  std::string title=file_path.filename().string();
  append_page(*hboxes.back(), title);
  show_all_children();
  set_current_page(Pages()-1);
  set_focus_child(*source_views.back()->view);
  CurrentSourceView()->get_buffer()->set_modified(false);
  //Add star on tab label when the page is not saved:
  auto source_view=CurrentSourceView();
  CurrentSourceView()->get_buffer()->signal_modified_changed().connect([this, source_view]() {
    boost::filesystem::path file_path(source_view->file_path);
    std::string title=file_path.filename().string();
    if(source_view->get_buffer()->get_modified())
      title+="*";
    int page=-1;
    for(int c=0;c<Pages();c++) {
      if(source_views.at(c)->view.get()==source_view) {
        page=c;
        break;
      }
    }
    if(page!=-1)
      set_tab_label_text(*(get_nth_page(page)), title);
  });
}

bool Notebook::close_current_page() {
  INFO("Notebook close page");
  if (Pages() != 0) {
    if(CurrentSourceView()->get_buffer()->get_modified()){
      if(!save_modified_dialog())
        return false;
    }
    int page = CurrentPage();
    remove_page(page);
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
  }
  return true;
}

Source::View* Notebook::CurrentSourceView() {
  INFO("Getting sourceview");
  return source_views.at(CurrentPage())->view.get();
}

int Notebook::CurrentPage() {
  return get_current_page();
}

int Notebook::Pages() {
  return get_n_pages();
}

bool Notebook::save_modified_dialog() {
  INFO("Notebook::save_dialog");
  Gtk::MessageDialog dialog((Gtk::Window&)(*get_toplevel()), "Save file!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_secondary_text("Do you want to save: " + CurrentSourceView()->file_path+" ?");
  int result = dialog.run();
  if(result==Gtk::RESPONSE_YES) {
    CurrentSourceView()->save();
    return true;
  }
  else if(result==Gtk::RESPONSE_NO) {
    return true;
  }
  else {
    return false;
  }
}

