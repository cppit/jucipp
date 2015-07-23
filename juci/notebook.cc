#include <fstream>
#include "notebook.h"
#include "logging.h"
#include "singletons.h"
#include <gtksourceview/gtksource.h> // c-library

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
  INFO("Notebook Controller Success");
}  // Constructor


void Notebook::Controller::CreateKeybindings() {
  auto menu=Singleton::menu();
  INFO("Notebook create signal handlers");
  directories.m_TreeView.signal_row_activated().connect(sigc::mem_fun(*this, &Notebook::Controller::OnDirectoryNavigation));

  menu->action_group->add(Gtk::Action::create("FileMenu", Gtk::Stock::FILE));

  menu->action_group->add(Gtk::Action::create("FileNewFile", "New file"), Gtk::AccelKey(menu->key_map["new_file"]), [this]() {
    OnFileNewFile();
  });
  menu->action_group->add(Gtk::Action::create("WindowCloseTab", "Close tab"), Gtk::AccelKey(menu->key_map["close_tab"]), [this]() {
    OnCloseCurrentPage();
  });
  //TODO: Wrap search in a class (together with replace) and make Source::View::search method (now it will probably crash if searching without tabs)
  //TODO: Do not update search/highlighting if searching for same string
  //TODO: Add custom style to search matches (gtk_source_search_context_set_match_style) and read this from config.json
  //TODO: Also update matches while writing in the search field
  //TODO: Also update cursor position
  menu->action_group->add(Gtk::Action::create("EditFind", "Find"), Gtk::AccelKey(menu->key_map["edit_find"]), [this]() {
    entry_box.clear();
    entry_box.entries.emplace_back("", [this](const std::string& content){
      search(content, true);
    });
    auto entry_it=entry_box.entries.begin();
    entry_box.buttons.emplace_back("Next", [this, entry_it](){
      search(entry_it->get_text(), true);
    });
    entry_box.buttons.emplace_back("Previous", [this, entry_it](){
      search(entry_it->get_text(), false);
    });
    entry_box.buttons.emplace_back("Cancel", [this](){
      entry_box.hide();
    });
    entry_box.signal_hide().connect([this]() {
      auto buffer=CurrentSourceView()->get_buffer();
      buffer->remove_tag_by_name("search", buffer->begin(), buffer->end());
      if(search_context!=NULL) {
        gtk_source_search_context_set_highlight(search_context, false);
      }
    });
    search_context=NULL; //TODO: delete content if any? Neither delete nor free worked... Do this on hide
    entry_box.show();
  });
  menu->action_group->add(Gtk::Action::create("EditCopy", "Copy"), Gtk::AccelKey(menu->key_map["edit_copy"]), [this]() {
    if (Pages() != 0) {
      CurrentSourceView()->get_buffer()->copy_clipboard(clipboard);
    }
  });
  menu->action_group->add(Gtk::Action::create("EditCut", "Cut"), Gtk::AccelKey(menu->key_map["edit_cut"]), [this]() {
	  if (Pages() != 0) {
	    CurrentSourceView()->get_buffer()->cut_clipboard(clipboard);
	  }
  });
  menu->action_group->add(Gtk::Action::create("EditPaste", "Paste"), Gtk::AccelKey(menu->key_map["edit_paste"]), [this]() {
	  if (Pages() != 0) {
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
  
  menu->action_group->add(Gtk::Action::create("SourceGotoDeclaration", "Go to declaration"), Gtk::AccelKey(menu->key_map["goto_declaration"]), [this]() {
    if(CurrentPage()!=-1) {
      if(CurrentSourceView()->get_declaration_location) {
        auto location=CurrentSourceView()->get_declaration_location();
        if(location.first.size()>0) {
          open_file(location.first);
          CurrentSourceView()->get_buffer()->place_cursor(CurrentSourceView()->get_buffer()->get_iter_at_offset(location.second));
          CurrentSourceView()->scroll_to(CurrentSourceView()->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        }
      }
    }
  });
  
  menu->action_group->add(Gtk::Action::create("SourceGotoMethod", "Go to method"), Gtk::AccelKey(menu->key_map["goto_method"]), [this]() {
    if(CurrentPage()!=-1) {
      if(CurrentSourceView()->goto_method) {
        CurrentSourceView()->goto_method();
      }
    }
  });

  INFO("Notebook signal handlers sucsess");
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
  CurrentSourceView()->get_buffer()->signal_modified_changed().connect([this]() {
    boost::filesystem::path file_path(CurrentSourceView()->file_path);
    std::string title=file_path.filename().string();
    if(CurrentSourceView()->get_buffer()->get_modified())
      title+="*";
    view.notebook.set_tab_label_text(*(view.notebook.get_nth_page(CurrentPage())), title);
  });
}

void Notebook::Controller::OnCloseCurrentPage() {
  INFO("Notebook close page");
  if (Pages() != 0) {
    if(CurrentSourceView()->get_buffer()->get_modified()){
      AskToSaveDialog();
    }
    int page = CurrentPage();
    view.notebook.remove_page(page);
    source_views.erase(source_views.begin()+ page);
    scrolled_windows.erase(scrolled_windows.begin()+page);
    hboxes.erase(hboxes.begin()+page);
  }
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
  entry_box.buttons.emplace_back("Cancel", [this](){
    entry_box.hide();
  });
  entry_box.show();
}

//TODO: see search TODO earlier
void Notebook::Controller::search(const std::string& text, bool forward) {
  INFO("Notebook search");
  if(search_context!=NULL)
    gtk_source_search_context_set_highlight(search_context, false);
  auto start = CurrentSourceView()->search_start;
  auto end = CurrentSourceView()->search_end;
  // fetch buffer and greate settings
  auto buffer = CurrentSourceView()->get_source_buffer();
  auto settings = gtk_source_search_settings_new();
  gtk_source_search_settings_set_search_text(settings, text.c_str());
  // make sure the search continues
  gtk_source_search_settings_set_wrap_around(settings, true);
  search_context = gtk_source_search_context_new(buffer->gobj(), settings);
  gtk_source_search_context_set_highlight(search_context, true);
  auto itr = buffer->get_insert()->get_iter();
  buffer->remove_tag_by_name("search", start ? start : itr, end ? end : itr);
  if (forward) {
    DEBUG("Doing forward search");
    gtk_source_search_context_forward(search_context,
				      end ? end.gobj() : itr.gobj(),
				      start.gobj(),
				      end.gobj());
  } else {
    DEBUG("Doing backward search");
    gtk_source_search_context_backward(search_context,
				       start ? start.gobj() : itr.gobj(),
				       start.gobj(),
				       end.gobj());
  }
  buffer->apply_tag_by_name("search", start, end);
  CurrentSourceView()->scroll_to(end);
  CurrentSourceView()->search_start = start;
  CurrentSourceView()->search_end = end;
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

bool Notebook::Controller:: OnSaveFile() {
  std::string path=CurrentSourceView()->file_path;
  return OnSaveFile(path);
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
      view.notebook.set_tab_label_text(*view.notebook.get_nth_page(CurrentPage()), title);
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

void Notebook::Controller::AskToSaveDialog() {
  INFO("AskToSaveDialog");
  DEBUG("AskToSaveDialog: Finding file path");
  Gtk::MessageDialog dialog((Gtk::Window&)(*view.get_toplevel()), "Save file!",
			    false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_secondary_text(
          "Do you want to save: " +
			    CurrentSourceView()->file_path+" ?");
  DEBUG("AskToSaveDialog: run dialog");
  int result = dialog.run();

  //Handle the response:
  DEBUG("AskToSaveDialog: switch response");
  switch(result)
  {
    case(Gtk::RESPONSE_YES):
    {
      DEBUG("AskToSaveDialog: save file: yes, trying to save file");
      OnSaveFile();
      DEBUG("AskToSaveDialog: save file: yes, saved sucess");
      break;
    }
    case(Gtk::RESPONSE_NO):
    {
      DEBUG("AskToSaveDialog: save file: no");
      break;
    }
    default:
    {
           DEBUG("AskToSaveDialog: unexpected action: Default switch");
      break;
    }
  }
}

