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
  clipboard = Gtk::Clipboard::get();
  view.pack1(directories.widget(), true, true);
  CreateKeybindings();
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
  menu->action_group->add(Gtk::Action::create("EditFind", "Find"), Gtk::AccelKey(menu->key_map["edit_find"]), [this]() {
	  entry.show_search("");
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

  entry.button_apply_set_filename.signal_clicked().connect([this]() {
    std::string filename=entry();
    if(filename!="") {
      if(project_path!="" && !boost::filesystem::path(filename).is_absolute())
        filename=project_path+"/"+filename;
      boost::filesystem::path p(filename);
      if(boost::filesystem::exists(p)) {
        //TODO: alert user that file already exists
      }
      else {
        std::ofstream f(p.string().c_str());
        if(f) {
          OnOpenFile(boost::filesystem::canonical(p).string());
          if(project_path!="")
            directories.open_folder(project_path); //TODO: Do refresh instead
        }
        else {
          //TODO: alert user of error creating file
        }
        f.close();
      }
    }
    entry.hide();
  });
  entry.button_close.signal_clicked().
    connect(
            [this]() {
              entry.hide();
            });
  entry.button_next.signal_clicked().
    connect(
            [this]() {
              search(true);
            });
  entry.button_prev.signal_clicked().
    connect(
            [this]() {
              search(false);
            });
  INFO("Notebook signal handlers sucsess");
}

void Notebook::Controller::OnOpenFile(std::string path) {
  INFO("Notebook open file");
  INFO("Notebook create page");
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
  entry.show_set_filename();
}

void Notebook::Controller::search(bool forward) {
  INFO("Notebook search");
  auto start = CurrentSourceView()->search_start;
  auto end = CurrentSourceView()->search_end;
  // fetch buffer and greate settings
  auto buffer = CurrentSourceView()->get_source_buffer();
  auto settings = gtk_source_search_settings_new();
  // get search text from entry
  gtk_source_search_settings_set_search_text(settings, entry().c_str());
  // make sure the search continues
  gtk_source_search_settings_set_wrap_around(settings, true);
  auto context = gtk_source_search_context_new(buffer->gobj(), settings);
  gtk_source_search_context_set_highlight(context, forward);
  auto itr = buffer->get_insert()->get_iter();
  buffer->remove_tag_by_name("search", start ? start : itr, end ? end : itr);
  if (forward) {
    DEBUG("Doing forward search");
    gtk_source_search_context_forward(context,
				      end ? end.gobj() : itr.gobj(),
				      start.gobj(),
				      end.gobj());
  } else {
    DEBUG("Doing backward search");
    gtk_source_search_context_backward(context,
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
      OnOpenFile(file);
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

