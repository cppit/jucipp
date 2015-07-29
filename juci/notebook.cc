#include <fstream>
#include "notebook.h"
#include "logging.h"
#include "singletons.h"
#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Notebook::View::View() {
  pack2(notebook);
  set_position(120);
}

Notebook::Controller::Controller() :
  directories() {
  INFO("Create notebook");
  Gsv::init();
  clipboard = Gtk::Clipboard::get();
  view.pack1(directories.widget(), true, true);
  CreateKeybindings();
  entry_box.signal_hide().connect([this]() {
    if(CurrentPage()!=-1) {
      CurrentSourceView()->grab_focus();
    }
  });
  view.notebook.signal_switch_page().connect([this](Gtk::Widget* page, guint page_num) {
    if(search_entry_shown && entry_box.labels.size()>0 && CurrentPage()!=-1) {
      CurrentSourceView()->update_search_occurrences=[this](int number){
        entry_box.labels.begin()->update(0, std::to_string(number));
      };
      CurrentSourceView()->search_highlight(last_search, case_sensitive_search, regex_search);
    }
    
    if(CurrentPage()!=-1) {
      if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceGotoDeclaration")))
        menu_item->set_sensitive((bool)CurrentSourceView()->get_declaration_location);
      
      if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceGotoMethod")))
        menu_item->set_sensitive((bool)CurrentSourceView()->goto_method);
      
      if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceRename")))
        menu_item->set_sensitive((bool)CurrentSourceView()->rename_similar_tokens);
    }
  });
  INFO("Notebook Controller Success");
}  // Constructor


void Notebook::Controller::CreateKeybindings() {
  auto menu=Singleton::menu();
  INFO("Notebook create signal handlers");
  directories.m_TreeView.signal_row_activated().connect(sigc::mem_fun(*this, &Notebook::Controller::OnDirectoryNavigation));

  menu->action_group->add(Gtk::Action::create("FileMenu", "File"));

  menu->action_group->add(Gtk::Action::create("FileNewFile", "New file"), Gtk::AccelKey(menu->key_map["new_file"]), [this]() {
    OnFileNewFile();
  });
  menu->action_group->add(Gtk::Action::create("WindowCloseTab", "Close tab"), Gtk::AccelKey(menu->key_map["close_tab"]), [this]() {
    close_current_page();
  });
  menu->action_group->add(Gtk::Action::create("EditFind", "Find"), Gtk::AccelKey(menu->key_map["edit_find"]), [this]() {
    show_search_and_replace();
  });
  menu->action_group->add(Gtk::Action::create("EditCopy", "Copy"), Gtk::AccelKey(menu->key_map["edit_copy"]), [this]() {
    auto window=(Gtk::Window*)view.get_toplevel();
    auto widget=window->get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->copy_clipboard();
    else if(auto text_view=dynamic_cast<Gtk::TextView*>(widget))
        text_view->get_buffer()->copy_clipboard(clipboard);
  });
  menu->action_group->add(Gtk::Action::create("EditCut", "Cut"), Gtk::AccelKey(menu->key_map["edit_cut"]), [this]() {
    auto window=(Gtk::Window*)view.get_toplevel();
    auto widget=window->get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->cut_clipboard();
    else {
      if (Pages() != 0)
        CurrentSourceView()->get_buffer()->cut_clipboard(clipboard);
    }
  });
  menu->action_group->add(Gtk::Action::create("EditPaste", "Paste"), Gtk::AccelKey(menu->key_map["edit_paste"]), [this]() {
    auto window=(Gtk::Window*)view.get_toplevel();
    auto widget=window->get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->paste_clipboard();
    else {
      if (Pages() != 0)
        CurrentSourceView()->get_buffer()->paste_clipboard(clipboard);
    }
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
  
  menu->action_group->add(Gtk::Action::create("SourceRename", "Rename function/variable"), Gtk::AccelKey(menu->key_map["source_rename"]), [this]() {
    entry_box.clear();
    if(CurrentPage()!=-1) {
      if(CurrentSourceView()->get_token && CurrentSourceView()->get_token_name) {
        auto token=std::make_shared<std::string>(CurrentSourceView()->get_token());
        if(token->size()>0 && CurrentSourceView()->get_token_name) {
          auto token_name=std::make_shared<std::string>(CurrentSourceView()->get_token_name());
          for(int c=0;c<Pages();c++) {
            if(source_views.at(c)->view->tag_similar_tokens) {
              source_views.at(c)->view->tag_similar_tokens(*token);
            }
          }
          entry_box.labels.emplace_back();
          auto label_it=entry_box.labels.begin();
          label_it->update=[label_it](int state, const std::string& message){
            label_it->set_text("Warning: only opened and parsed tabs will have its content renamed, and modified files will be saved.");
          };
          label_it->update(0, "");
          entry_box.entries.emplace_back(*token_name, [this, token_name, token](const std::string& content){
            if(CurrentPage()!=-1 && content!=*token_name) {
              for(int c=0;c<Pages();c++) {
                if(source_views.at(c)->view->rename_similar_tokens) {
                  auto number=source_views.at(c)->view->rename_similar_tokens(*token, content);
                  if(number>0) {
                    Singleton::terminal()->print("Replaced "+std::to_string(number)+" occurrences in file "+source_views.at(c)->view->file_path+"\n");
                    source_views.at(c)->view->save();
                  }
                }
              }
              entry_box.hide();
            }
          });
          auto entry_it=entry_box.entries.begin();
          entry_box.buttons.emplace_back("Rename", [this, entry_it](){
            entry_it->activate();
          });
          entry_box.show();
        }
      }
    }
  });

  INFO("Notebook signal handlers sucsess");
}

void Notebook::Controller::show_search_and_replace() {
  entry_box.clear();
  entry_box.labels.emplace_back();
  auto label_it=entry_box.labels.begin();
  label_it->update=[label_it](int state, const std::string& message){
    if(state==0) {
      int number=stoi(message);
      if(number==0)
        label_it->set_text("");
      else if(number==1)
        label_it->set_text("1 result found");
      else if(number>1)
        label_it->set_text(std::to_string(number)+" results found");
    }
  };
  entry_box.entries.emplace_back(last_search, [this](const std::string& content){
    if(CurrentPage()!=-1)
      CurrentSourceView()->search_forward();
  });
  auto search_entry_it=entry_box.entries.begin();
  search_entry_it->set_placeholder_text("Find");
  if(CurrentPage()!=-1) {
    CurrentSourceView()->update_search_occurrences=[label_it](int number){
      label_it->update(0, std::to_string(number));
    };
    CurrentSourceView()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  }
  search_entry_it->signal_key_press_event().connect([this](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && event->state==GDK_SHIFT_MASK) {
      if(CurrentPage()!=-1)
        CurrentSourceView()->search_backward();
    }
    return false;
  });
  search_entry_it->signal_changed().connect([this, search_entry_it](){
    last_search=search_entry_it->get_text();
    if(CurrentPage()!=-1)
      CurrentSourceView()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  });
  
  entry_box.entries.emplace_back(last_replace, [this](const std::string &content){
    if(CurrentPage()!=-1)
      CurrentSourceView()->replace_forward(content);
  });
  auto replace_entry_it=entry_box.entries.begin();
  replace_entry_it++;
  replace_entry_it->set_placeholder_text("Replace");
  replace_entry_it->signal_key_press_event().connect([this, replace_entry_it](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && event->state==GDK_SHIFT_MASK) {
      if(CurrentPage()!=-1)
        CurrentSourceView()->replace_backward(replace_entry_it->get_text());
    }
    return false;
  });
  replace_entry_it->signal_changed().connect([this, replace_entry_it](){
    last_replace=replace_entry_it->get_text();
  });
  
  entry_box.buttons.emplace_back("Find", [this](){
    if(CurrentPage()!=-1)
      CurrentSourceView()->search_forward();
  });
  entry_box.buttons.emplace_back("Replace", [this, replace_entry_it](){
    if(CurrentPage()!=-1)
      CurrentSourceView()->replace_forward(replace_entry_it->get_text());
  });
  entry_box.buttons.emplace_back("Replace all", [this, replace_entry_it](){
    if(CurrentPage()!=-1)
      CurrentSourceView()->replace_all(replace_entry_it->get_text());
  });
  entry_box.toggle_buttons.emplace_back("Match case");
  entry_box.toggle_buttons.back().set_active(case_sensitive_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    case_sensitive_search=!case_sensitive_search;
    if(CurrentPage()!=-1)
      CurrentSourceView()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.toggle_buttons.emplace_back("Use regex");
  entry_box.toggle_buttons.back().set_active(regex_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    regex_search=!regex_search;
    if(CurrentPage()!=-1)
      CurrentSourceView()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.signal_hide().connect([this]() {
    for(int c=0;c<Pages();c++) {
      source_views.at(c)->view->update_search_occurrences=nullptr;
      source_views.at(c)->view->search_highlight("", case_sensitive_search, regex_search);
    }
    search_entry_shown=false;
  });
  search_entry_shown=true;
  entry_box.show();
}

void Notebook::Controller::open_file(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
  for(int c=0;c<Pages();c++) {
    if(path==source_views.at(c)->view->file_path) {
      view.notebook.set_current_page(c);
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
  view.notebook.append_page(*hboxes.back(), title);
  view.notebook.show_all_children();
  view.notebook.set_current_page(Pages()-1);
  view.notebook.set_focus_child(*source_views.back()->view);
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
      view.notebook.set_tab_label_text(*(view.notebook.get_nth_page(page)), title);
  });
}

bool Notebook::Controller::close_current_page() {
  INFO("Notebook close page");
  if (Pages() != 0) {
    if(CurrentSourceView()->get_buffer()->get_modified()){
      if(!save_dialog())
        return false;
    }
    int page = CurrentPage();
    view.notebook.remove_page(page);
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
  }
  return true;
}
void Notebook::Controller::OnFileNewFile() {
  entry_box.clear();
  entry_box.entries.emplace_back("untitled", [this](const std::string& content){
    std::string filename=content;
    if(filename!="") {
      if(project_path!="" && !boost::filesystem::path(filename).is_absolute())
        filename=project_path+"/"+filename;
      boost::filesystem::path p(filename);
      if(boost::filesystem::exists(p)) {
        Singleton::terminal()->print("Error: "+p.string()+" already exists.\n");
      }
      else {
        std::ofstream f(p.string().c_str());
        if(f) {
          open_file(boost::filesystem::canonical(p).string());
          Singleton::terminal()->print("New file "+p.string()+" created.\n");
          if(project_path!="")
            directories.open_folder(project_path); //TODO: Do refresh instead
        }
        else {
          Singleton::terminal()->print("Error: could not create new file "+p.string()+".\n");
        }
        f.close();
      }
    }
    entry_box.hide();
  });
  auto entry_it=entry_box.entries.begin();
  entry_box.buttons.emplace_back("Create file", [this, entry_it](){
    entry_it->activate();
  });
  entry_box.show();
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
      open_file(file);
    }
  }
}

Source::View* Notebook::Controller::CurrentSourceView() {
  INFO("Getting sourceview");
  return source_views.at(CurrentPage())->view.get();
}

int Notebook::Controller::CurrentPage() {
  return view.notebook.get_current_page();
}

int Notebook::Controller::Pages() {
  return view.notebook.get_n_pages();
}

bool Notebook::Controller:: OnSaveFile(std::string path) {
    INFO("Notebook save file with path");
    if (path != "" && CurrentSourceView()->get_buffer()->get_modified()) {
      std::ofstream file;
      file.open (path);
      file << CurrentSourceView()->get_buffer()->get_text();
      file.close();
      boost::filesystem::path path(CurrentSourceView()->file_path);
      std::string title=path.filename().string();
      CurrentSourceView()->get_buffer()->set_modified(false);
      return true;
    }
  return false;
}


std::string Notebook::Controller::OnSaveFileAs(){
  INFO("Notebook save as");
  Gtk::FileChooserDialog dialog((Gtk::Window&)(*view.get_toplevel()), "Please choose a file", 
				Gtk::FILE_CHOOSER_ACTION_SAVE);
  DEBUG("SET TRANSISTEN FPR");
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

bool Notebook::Controller::save_dialog() {
  INFO("Notebook::Controller::save_dialog");
  Gtk::MessageDialog dialog((Gtk::Window&)(*view.get_toplevel()), "Save file!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
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

