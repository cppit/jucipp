#include "window.h"
#include "logging.h"
#include "singletons.h"
#include "sourcefile.h"
#include "config.h"
#include "api.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Window::Window() : box(Gtk::ORIENTATION_VERTICAL) {
  INFO("Create Window");
  set_title("juCi++");
  set_default_size(600, 400);
  set_events(Gdk::POINTER_MOTION_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::SCROLL_MASK);
  add(box);

  MainConfig(); //Read the configs here
  PluginApi(&this->notebook); //Initialise plugins
  add_menu();

  box.pack_start(entry_box, Gtk::PACK_SHRINK);

  directory_and_notebook_panes.pack1(directories, true, true); //TODO: should be pack1(directories, ...) Clean up directories.*
  directory_and_notebook_panes.pack2(notebook);
  directory_and_notebook_panes.set_position(120);

  vpaned.set_position(300);
  vpaned.pack1(directory_and_notebook_panes, true, false);
  vpaned.pack2(Singleton::terminal()->view, true, true);
  box.pack_end(vpaned);
  show_all_children();

  directories.on_row_activated=[this](const std::string &file) {
    notebook.open(file);
  };

  entry_box.signal_show().connect([this](){
      std::vector<Gtk::Widget*> focus_chain;
      focus_chain.emplace_back(&entry_box);
      box.set_focus_chain(focus_chain);
    });
  entry_box.signal_hide().connect([this](){
      box.unset_focus_chain();
    });
  entry_box.signal_hide().connect([this]() {
      if(notebook.get_current_page()!=-1) {
        notebook.get_current_view()->grab_focus();
      }
    });

  notebook.signal_switch_page().connect([this](Gtk::Widget* page, guint page_num) {
      if(search_entry_shown && entry_box.labels.size()>0 && notebook.get_current_page()!=-1) {
        notebook.get_current_view()->update_search_occurrences=[this](int number){
          entry_box.labels.begin()->update(0, std::to_string(number));
        };
        notebook.get_current_view()->search_highlight(last_search, case_sensitive_search, regex_search);
      }

      if(notebook.get_current_page()!=-1) {
        if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceGotoDeclaration")))
          menu_item->set_sensitive((bool)notebook.get_current_view()->get_declaration_location);

        if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceGotoMethod")))
          menu_item->set_sensitive((bool)notebook.get_current_view()->goto_method);

        if(auto menu_item=dynamic_cast<Gtk::MenuItem*>(Singleton::menu()->ui_manager->get_widget("/MenuBar/SourceMenu/SourceRename")))
          menu_item->set_sensitive((bool)notebook.get_current_view()->rename_similar_tokens);
      }
    });
  INFO("Window created");
} // Window constructor

void Window::add_menu() {
  auto menu=Singleton::menu();
  INFO("Adding actions to menu");
  menu->action_group->add(Gtk::Action::create("FileQuit", "Quit juCi++"), Gtk::AccelKey(menu->key_map["quit"]), [this]() {
      hide();
    });
  menu->action_group->add(Gtk::Action::create("FileNewFile", "New file"), Gtk::AccelKey(menu->key_map["new_file"]), [this]() {
      new_file_entry();
    });
  menu->action_group->add(Gtk::Action::create("FileOpenFile", "Open file"), Gtk::AccelKey(menu->key_map["open_file"]), [this]() {
      open_file_dialog();
    });
  menu->action_group->add(Gtk::Action::create("FileOpenFolder", "Open folder"), Gtk::AccelKey(menu->key_map["open_folder"]), [this]() {
      open_folder_dialog();
    });
  menu->action_group->add(Gtk::Action::create("FileSaveAs", "Save as"), Gtk::AccelKey(menu->key_map["save_as"]), [this]() {
      save_file_dialog();
    });

  menu->action_group->add(Gtk::Action::create("FileSave", "Save"), Gtk::AccelKey(menu->key_map["save"]), [this]() {
      notebook.save_current();
    });

  menu->action_group->add(Gtk::Action::create("EditCopy", "Copy"), Gtk::AccelKey(menu->key_map["edit_copy"]), [this]() {
      auto widget=get_focus();
      if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
        entry->copy_clipboard();
      else if(auto text_view=dynamic_cast<Gtk::TextView*>(widget))
        text_view->get_buffer()->copy_clipboard(Gtk::Clipboard::get());
    });
  menu->action_group->add(Gtk::Action::create("EditCut", "Cut"), Gtk::AccelKey(menu->key_map["edit_cut"]), [this]() {
      auto widget=get_focus();
      if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
        entry->cut_clipboard();
      else if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->get_buffer()->cut_clipboard(Gtk::Clipboard::get());
    });
  menu->action_group->add(Gtk::Action::create("EditPaste", "Paste"), Gtk::AccelKey(menu->key_map["edit_paste"]), [this]() {
      auto widget=get_focus();
      if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
        entry->paste_clipboard();
      else if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->get_buffer()->paste_clipboard(Gtk::Clipboard::get());
    });
  menu->action_group->add(Gtk::Action::create("EditFind", "Find"), Gtk::AccelKey(menu->key_map["edit_find"]), [this]() {
      search_and_replace_entry();
    });
  menu->action_group->add(Gtk::Action::create("EditUndo", "Undo"), Gtk::AccelKey(menu->key_map["edit_undo"]), [this]() {
      INFO("On undo");
      if(notebook.get_current_page()!=-1) {
        auto undo_manager = notebook.get_current_view()->get_source_buffer()->get_undo_manager();
        if (undo_manager->can_undo()) {
          undo_manager->undo();
        }
      }
      INFO("Done undo");
    });
  menu->action_group->add(Gtk::Action::create("EditRedo", "Redo"), Gtk::AccelKey(menu->key_map["edit_redo"]), [this]() {
      INFO("On Redo");
      if(notebook.get_current_page()!=-1) {
        auto undo_manager = notebook.get_current_view()->get_source_buffer()->get_undo_manager();
        if(undo_manager->can_redo()) {
          undo_manager->redo();
        }
      }
      INFO("Done Redo");
    });

  menu->action_group->add(Gtk::Action::create("SourceGotoDeclaration", "Go to declaration"), Gtk::AccelKey(menu->key_map["source_goto_declaration"]), [this]() {
      if(notebook.get_current_page()!=-1) {
        if(notebook.get_current_view()->get_declaration_location) {
          auto location=notebook.get_current_view()->get_declaration_location();
          if(location.first.size()>0) {
            notebook.open(location.first);
            notebook.get_current_view()->get_buffer()->place_cursor(notebook.get_current_view()->get_buffer()->get_iter_at_offset(location.second));
            while(gtk_events_pending())
              gtk_main_iteration();
            notebook.get_current_view()->scroll_to(notebook.get_current_view()->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
          }
        }
      }
    });
  menu->action_group->add(Gtk::Action::create("SourceGotoMethod", "Go to method"), Gtk::AccelKey(menu->key_map["source_goto_method"]), [this]() {
      if(notebook.get_current_page()!=-1) {
        if(notebook.get_current_view()->goto_method) {
          notebook.get_current_view()->goto_method();
        }
      }
    });
  menu->action_group->add(Gtk::Action::create("SourceRename", "Rename"), Gtk::AccelKey(menu->key_map["source_rename"]), [this]() {
      rename_token_entry();
    });

  menu->action_group->add(Gtk::Action::create("ProjectCompileAndRun", "Compile And Run"), Gtk::AccelKey(menu->key_map["compile_and_run"]), [this]() {
      if(notebook.get_current_page()==-1)
        return;
      notebook.save_current();
      if (running.try_lock()) {
        std::thread execute([this]() {
            std::string path = notebook.get_current_view()->file_path;
            size_t pos = path.find_last_of("/\\");
            if(pos != std::string::npos) {
              path.erase(path.begin()+pos,path.end());
              Singleton::terminal()->SetFolderCommand(path);
            }
            Singleton::terminal()->Compile();
            std::string executable = directories.get_cmakelists_variable(path,"add_executable");
            Singleton::terminal()->Run(executable);
            running.unlock();
          });
        execute.detach();
      }
    });
  menu->action_group->add(Gtk::Action::create("ProjectCompile", "Compile"), Gtk::AccelKey(menu->key_map["compile"]), [this]() {
      if(notebook.get_current_page()==-1)
        return;
      notebook.save_current();
      if (running.try_lock()) {
        std::thread execute([this]() {
            std::string path = notebook.get_current_view()->file_path;
            size_t pos = path.find_last_of("/\\");
            if(pos != std::string::npos){
              path.erase(path.begin()+pos,path.end());
              Singleton::terminal()->SetFolderCommand(path);
            }
            Singleton::terminal()->Compile();
            running.unlock();
          });
        execute.detach();
      }
    });

  menu->action_group->add(Gtk::Action::create("WindowCloseTab", "Close tab"), Gtk::AccelKey(menu->key_map["close_tab"]), [this]() {
      notebook.close_current_page();
    });
  add_accel_group(menu->ui_manager->get_accel_group());
  menu->build();
  INFO("Menu build")
    box.pack_start(menu->get_widget(), Gtk::PACK_SHRINK);
}

bool Window::on_key_press_event(GdkEventKey *event) {
  if(event->keyval==GDK_KEY_Escape)
    entry_box.hide();
#ifdef __APPLE__ //For Apple's Command-left, right, up, down keys
  else if((event->state & GDK_META_MASK)>0) {
    if(event->keyval==GDK_KEY_Left) {
      event->keyval=GDK_KEY_Home;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=115;
    }
    else if(event->keyval==GDK_KEY_Right) {
      event->keyval=GDK_KEY_End;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=119;
    }
    else if(event->keyval==GDK_KEY_Up) {
      if(auto text_view=dynamic_cast<Gtk::TextView*>(get_focus())) {
        if(event->state & GDK_SHIFT_MASK)
          text_view->get_buffer()->select_range(text_view->get_buffer()->begin(), text_view->get_buffer()->get_insert()->get_iter());
        else
          text_view->get_buffer()->place_cursor(text_view->get_buffer()->begin());
        text_view->scroll_to(text_view->get_buffer()->get_insert());
      }
      return true;
    }
    else if(event->keyval==GDK_KEY_Down) {
      if(auto text_view=dynamic_cast<Gtk::TextView*>(get_focus())) {
        if(event->state & GDK_SHIFT_MASK)
          text_view->get_buffer()->select_range(text_view->get_buffer()->end(), text_view->get_buffer()->get_insert()->get_iter());
        else
          text_view->get_buffer()->place_cursor(text_view->get_buffer()->end());
        text_view->scroll_to(text_view->get_buffer()->get_insert());
      }
      return true;
    }
  }
#endif

  return Gtk::Window::on_key_press_event(event);
}

bool Window::on_delete_event (GdkEventAny *event) {
  hide();
  return true;
}

void Window::hide() {
  auto size=notebook.size();
  for(int c=0;c<size;c++) {
    if(!notebook.close_current_page())
      return;
  }
  Gtk::Window::hide();
}

void Window::new_file_entry() {
  entry_box.clear();
  entry_box.entries.emplace_back("untitled", [this](const std::string& content){
    std::string filename=content;
    if(filename!="") {
      if(notebook.project_path!="" && !boost::filesystem::path(filename).is_absolute())
        filename=notebook.project_path+"/"+filename;
      boost::filesystem::path p(filename);
      if(boost::filesystem::exists(p)) {
        Singleton::terminal()->print("Error: "+p.string()+" already exists.\n");
      }
      else {
        juci::filesystem::save(p);
        notebook.open(boost::filesystem::canonical(p).string());
        Singleton::terminal()->print("New file "+p.string()+" created.\n");
        if(notebook.project_path!="")
           directories.open_folder(notebook.project_path); //TODO: Do refresh instead
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

void Window::open_folder_dialog() {
  Gtk::FileChooserDialog dialog("Please choose a folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
  if(notebook.project_path.size()>0)
    gtk_file_chooser_set_current_folder((GtkFileChooser*)dialog.gobj(), notebook.project_path.c_str());
  dialog.set_transient_for(*this);
  //Add response buttons the the dialog:
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("Select", Gtk::RESPONSE_OK);

  int result = dialog.run();

  if(result==Gtk::RESPONSE_OK) {
    std::string project_path=dialog.get_filename();
    notebook.project_path=project_path;
    directories.open_folder(project_path);
  }
}

void Window::open_file_dialog() {
  Gtk::FileChooserDialog dialog("Please choose a file", Gtk::FILE_CHOOSER_ACTION_OPEN);
  if(notebook.project_path.size()>0)
    gtk_file_chooser_set_current_folder((GtkFileChooser*)dialog.gobj(), notebook.project_path.c_str());
  dialog.set_transient_for(*this);
  dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);

  //Add response buttons the the dialog:
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Open", Gtk::RESPONSE_OK);

  //Add filters, so that only certain file types can be selected:
  Glib::RefPtr<Gtk::FileFilter> filter_text = Gtk::FileFilter::create();
  filter_text->set_name("Text files");
  filter_text->add_mime_type("text/plain");
  dialog.add_filter(filter_text);

  Glib::RefPtr<Gtk::FileFilter> filter_cpp = Gtk::FileFilter::create();
  filter_cpp->set_name("C/C++ files");
  filter_cpp->add_mime_type("text/x-c");
  filter_cpp->add_mime_type("text/x-c++");
  filter_cpp->add_mime_type("text/x-c-header");
  dialog.add_filter(filter_cpp);

  Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
  filter_any->set_name("Any files");
  filter_any->add_pattern("*");
  dialog.add_filter(filter_any);

  int result = dialog.run();

  if(result==Gtk::RESPONSE_OK) {
    std::string path = dialog.get_filename();
    notebook.open(path);
  }
}

void Window::save_file_dialog() {
  if(notebook.get_current_page()==-1)
    return;
  INFO("Save file dialog");
  Gtk::FileChooserDialog dialog(*this, "Please choose a file", Gtk::FILE_CHOOSER_ACTION_SAVE);
  gtk_file_chooser_set_filename((GtkFileChooser*)dialog.gobj(), notebook.get_current_view()->file_path.c_str());
  dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Save", Gtk::RESPONSE_OK);

  int result = dialog.run();
  if(result==Gtk::RESPONSE_OK) {
    auto path = dialog.get_filename();
    if(path.size()>0) {
      std::ofstream file(path);
      if(file) {
        file << notebook.get_current_view()->get_buffer()->get_text();
        file.close();
        notebook.open(path);
        Singleton::terminal()->print("File saved to: " + notebook.get_current_view()->file_path+"\n");
        if(notebook.project_path!="")
          directories.open_folder(notebook.project_path); //TODO: Do refresh instead
      }
      else
        Singleton::terminal()->print("Error saving file\n");
    }
  }
}

void Window::search_and_replace_entry() {
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
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->search_forward();
    });
  auto search_entry_it=entry_box.entries.begin();
  search_entry_it->set_placeholder_text("Find");
  if(notebook.get_current_page()!=-1) {
    notebook.get_current_view()->update_search_occurrences=[label_it](int number){
      label_it->update(0, std::to_string(number));
    };
    notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  }
  search_entry_it->signal_key_press_event().connect([this](GdkEventKey* event){
      if(event->keyval==GDK_KEY_Return && event->state==GDK_SHIFT_MASK) {
        if(notebook.get_current_page()!=-1)
          notebook.get_current_view()->search_backward();
      }
      return false;
    });
  search_entry_it->signal_changed().connect([this, search_entry_it](){
      last_search=search_entry_it->get_text();
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
    });

  entry_box.entries.emplace_back(last_replace, [this](const std::string &content){
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->replace_forward(content);
    });
  auto replace_entry_it=entry_box.entries.begin();
  replace_entry_it++;
  replace_entry_it->set_placeholder_text("Replace");
  replace_entry_it->signal_key_press_event().connect([this, replace_entry_it](GdkEventKey* event){
      if(event->keyval==GDK_KEY_Return && event->state==GDK_SHIFT_MASK) {
        if(notebook.get_current_page()!=-1)
          notebook.get_current_view()->replace_backward(replace_entry_it->get_text());
      }
      return false;
    });
  replace_entry_it->signal_changed().connect([this, replace_entry_it](){
      last_replace=replace_entry_it->get_text();
    });

  entry_box.buttons.emplace_back("Find", [this](){
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->search_forward();
    });
  entry_box.buttons.emplace_back("Replace", [this, replace_entry_it](){
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->replace_forward(replace_entry_it->get_text());
    });
  entry_box.buttons.emplace_back("Replace all", [this, replace_entry_it](){
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->replace_all(replace_entry_it->get_text());
    });
  entry_box.toggle_buttons.emplace_back("Match case");
  entry_box.toggle_buttons.back().set_active(case_sensitive_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    case_sensitive_search=!case_sensitive_search;
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.toggle_buttons.emplace_back("Use regex");
  entry_box.toggle_buttons.back().set_active(regex_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    regex_search=!regex_search;
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.signal_hide().connect([this]() {
      for(int c=0;c<notebook.size();c++) {
        notebook.get_view(c)->update_search_occurrences=nullptr;
        notebook.get_view(c)->search_highlight("", case_sensitive_search, regex_search);
      }
      search_entry_shown=false;
    });
  search_entry_shown=true;
  entry_box.show();
}

void Window::rename_token_entry() {
  entry_box.clear();
  if(notebook.get_current_page()!=-1) {
    if(notebook.get_current_view()->get_token && notebook.get_current_view()->get_token_name) {
      auto token=std::make_shared<std::string>(notebook.get_current_view()->get_token());
      if(token->size()>0 && notebook.get_current_view()->get_token_name) {
        auto token_name=std::make_shared<std::string>(notebook.get_current_view()->get_token_name());
        for(int c=0;c<notebook.size();c++) {
          if(notebook.get_view(c)->tag_similar_tokens) {
            notebook.get_view(c)->tag_similar_tokens(*token);
          }
        }
        entry_box.labels.emplace_back();
        auto label_it=entry_box.labels.begin();
        label_it->update=[label_it](int state, const std::string& message){
          label_it->set_text("Warning: only opened and parsed tabs will have its content renamed, and modified files will be saved.");
        };
        label_it->update(0, "");
        entry_box.entries.emplace_back(*token_name, [this, token_name, token](const std::string& content){
            if(notebook.get_current_page()!=-1 && content!=*token_name) {
              for(int c=0;c<notebook.size();c++) {
                if(notebook.get_view(c)->rename_similar_tokens) {
                  auto number=notebook.get_view(c)->rename_similar_tokens(*token, content);
                  if(number>0) {
                    Singleton::terminal()->print("Replaced "+std::to_string(number)+" occurrences in file "+notebook.get_view(c)->file_path+"\n");
                    notebook.save(c);
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
}
