#include "window.h"
#include "logging.h"
#include "singletons.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Window::Window() :
  window_box_(Gtk::ORIENTATION_VERTICAL) {
  INFO("Create Window");
  set_title("juCi++");
  set_default_size(600, 400);
  set_events(Gdk::POINTER_MOTION_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::SCROLL_MASK);
  add(window_box_);
  auto menu=Singleton::menu();
  menu->action_group->add(Gtk::Action::create("FileQuit", "Quit juCi++"), Gtk::AccelKey(menu->key_map["quit"]), [this]() {
    hide();
  });
  menu->action_group->add(Gtk::Action::create("FileOpenFile", "Open file"), Gtk::AccelKey(menu->key_map["open_file"]), [this]() {
    OnOpenFile();
  });
  menu->action_group->add(Gtk::Action::create("FileOpenFolder", "Open folder"), Gtk::AccelKey(menu->key_map["open_folder"]), [this]() {
    OnFileOpenFolder();
  });
  menu->action_group->add(Gtk::Action::create("FileSaveAs", "Save as"), Gtk::AccelKey(menu->key_map["save_as"]), [this]() {
    SaveFileAs();
  });

  menu->action_group->add(Gtk::Action::create("FileSave", "Save"), Gtk::AccelKey(menu->key_map["save"]), [this]() {
    SaveFile();
  });
  
  menu->action_group->add(Gtk::Action::create("ProjectCompileAndRun", "Compile And Run"), Gtk::AccelKey(menu->key_map["compile_and_run"]), [this]() {
	  SaveFile();
	  if (running.try_lock()) {
	    std::thread execute([this]() {
		std::string path = Singleton::notebook()->CurrentSourceView()->file_path;
		size_t pos = path.find_last_of("/\\");
		if(pos != std::string::npos) {
		  path.erase(path.begin()+pos,path.end());
		  Singleton::terminal()->SetFolderCommand(path);
		}
		Singleton::terminal()->Compile();
		std::string executable = Singleton::notebook()->directories.
		  GetCmakeVarValue(path,"add_executable");
		Singleton::terminal()->Run(executable);
                running.unlock();
	      });
	    execute.detach();
	  }
	});
   
  menu->action_group->add(Gtk::Action::create("ProjectCompile", "Compile"), Gtk::AccelKey(menu->key_map["compile"]), [this]() {
    SaveFile();
    if (running.try_lock()) {
      std::thread execute([this]() {		  
          std::string path = Singleton::notebook()->CurrentSourceView()->file_path;
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

  add_accel_group(menu->ui_manager->get_accel_group());
  menu->build();

  window_box_.pack_start(menu->get_widget(), Gtk::PACK_SHRINK);

  window_box_.pack_start(Singleton::notebook()->entry_box, Gtk::PACK_SHRINK);
  paned_.set_position(300);
  paned_.pack1(Singleton::notebook()->view, true, false);
  paned_.pack2(Singleton::terminal()->view, true, true);
  window_box_.pack_end(paned_);
  show_all_children();
  
  Singleton::notebook()->entry_box.signal_show().connect([this](){
    std::vector<Gtk::Widget*> focus_chain;
    focus_chain.emplace_back(&Singleton::notebook()->entry_box);
    window_box_.set_focus_chain(focus_chain);
  });
  Singleton::notebook()->entry_box.signal_hide().connect([this](){
    window_box_.unset_focus_chain();
  });
  
  INFO("Window created");
} // Window constructor

bool Window::on_key_press_event(GdkEventKey *event) {
  if(event->keyval==GDK_KEY_Escape)
    Singleton::notebook()->entry_box.hide();
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
  auto size=Singleton::notebook()->source_views.size();
  for(size_t c=0;c<size;c++) {
    if(!Singleton::notebook()->close_current_page())
      return;
  }
  Gtk::Window::hide();
}

void Window::OnFileOpenFolder() {
  Gtk::FileChooserDialog dialog("Please choose a folder",
                                Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
  dialog.set_transient_for(*this);
  //Add response buttons the the dialog:
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("Select", Gtk::RESPONSE_OK);

  int result = dialog.run();

  //Handle the response:
  switch(result)
    {
    case(Gtk::RESPONSE_OK):
      {
        std::string project_path=dialog.get_filename();
        Singleton::notebook()->project_path=project_path;
        Singleton::notebook()->directories.open_folder(project_path);
        break;
      }
    case(Gtk::RESPONSE_CANCEL):
      {
        break;
      }
    default:
      {
        break;
      }
    }
}


void Window::OnOpenFile() {
  Gtk::FileChooserDialog dialog("Please choose a file",
                                Gtk::FILE_CHOOSER_ACTION_OPEN);
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

  switch (result) {
  case(Gtk::RESPONSE_OK): {
    std::string path = dialog.get_filename();
    Singleton::notebook()->open_file(path);
    break;
  }
  case(Gtk::RESPONSE_CANCEL): {
    break;
  }
  default: {
    break;
  }
  }
}

//TODO: Move to notebook. Maybe also replace bool with void?
bool Window::SaveFile() {
  return Singleton::notebook()->CurrentSourceView()->save();
}
bool Window::SaveFileAs() {
  if(Singleton::notebook()->OnSaveFile(Singleton::notebook()->OnSaveFileAs())){
    Singleton::terminal()->print("File saved to: " +
			   Singleton::notebook()->CurrentSourceView()->file_path+"\n");
    return true;
  }
  Singleton::terminal()->print("File not saved");
  return false;
}
