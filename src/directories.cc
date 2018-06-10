#include "directories.h"
#include <algorithm>
#include "source.h"
#include "terminal.h"
#include "notebook.h"
#include "filesystem.h"
#include "entrybox.h"

bool Directories::TreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path &path, const Gtk::SelectionData &selection_data) const {
  return true;
}

bool Directories::TreeStore::drag_data_received_vfunc(const TreeModel::Path &path, const Gtk::SelectionData &selection_data) {
  auto &directories=Directories::get();
  
  auto get_target_folder=[this, &directories](const TreeModel::Path &path) {
    if(path.size()==1)
      return directories.path;
    else {
      auto it=get_iter(path);
      if(it) {
        auto prev_path=path;
        prev_path.up();
        it=get_iter(prev_path);
        if(it)
          return it->get_value(directories.column_record.path);
      }
      else {
        auto prev_path=path;
        prev_path.up();
        if(prev_path.size()==1)
          return directories.path;
        else {
          prev_path.up();
          it=get_iter(prev_path);
          if(it)
            return it->get_value(directories.column_record.path);
        }
      }
    }
    return boost::filesystem::path();
  };
  
  auto it=directories.get_selection()->get_selected();
  if(it) {
    auto source_path=it->get_value(directories.column_record.path);
    if(source_path.empty())
      return false;
    
    auto target_path=get_target_folder(path);
    target_path/=source_path.filename();
    
    if(source_path==target_path)
      return false;
    
    if(boost::filesystem::exists(target_path)) {
      Terminal::get().print("Error: could not move file: "+target_path.string()+" already exists\n", true);
      return false;
    }
    
    bool is_directory=boost::filesystem::is_directory(source_path);
    
    if(is_directory)
      Directories::get().remove_path(source_path);
    
    boost::system::error_code ec;
    boost::filesystem::rename(source_path, target_path, ec);
    if(ec) {
      Terminal::get().print("Error: could not move file: "+ec.message()+'\n', true);
      return false;
    }
    
    for(size_t c=0;c<Notebook::get().size();c++) {
      auto view=Notebook::get().get_view(c);
      if(is_directory) {
        if(filesystem::file_in_path(view->file_path, source_path)) {
          auto file_it=view->file_path.begin();
          for(auto source_it=source_path.begin();source_it!=source_path.end();source_it++)
            file_it++;
          auto new_file_path=target_path;
          for(;file_it!=view->file_path.end();file_it++)
            new_file_path/=*file_it;
          view->rename(new_file_path);
        }
      }
      else if(view->file_path==source_path) {
        view->rename(target_path);
        break;
      }
    }
    
    Directories::get().update();
    Directories::get().on_save_file(target_path);
    directories.select(target_path);
  }
  
  EntryBox::get().hide();
  return false;
}

bool Directories::TreeStore::drag_data_delete_vfunc (const Gtk::TreeModel::Path &path) {
  return false;
}

Directories::Directories() : Gtk::ListViewText(1) {
  set_enable_tree_lines(true);
  
  tree_store = TreeStore::create();
  tree_store->set_column_types(column_record);
  set_model(tree_store);
  
  get_column(0)->set_title("");
  
  auto renderer=dynamic_cast<Gtk::CellRendererText*>(get_column(0)->get_first_cell());
  get_column(0)->set_cell_data_func(*renderer, [this] (Gtk::CellRenderer *renderer, const Gtk::TreeModel::iterator &iter) {
    if(auto renderer_text=dynamic_cast<Gtk::CellRendererText*>(renderer))
      renderer_text->property_markup()=iter->get_value(column_record.markup);
  });
  
  get_style_context()->add_class("juci_directories");
  
  tree_store->set_sort_column(column_record.id, Gtk::SortType::SORT_ASCENDING);
  set_enable_search(true); //TODO: why does this not work in OS X?
  set_search_column(column_record.name);
  
  signal_row_activated().connect([this](const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *column){
    auto iter = tree_store->get_iter(path);
    if (iter) {
      auto filesystem_path=iter->get_value(column_record.path);
      if(filesystem_path!="") {
        if (boost::filesystem::is_directory(boost::filesystem::path(filesystem_path)))
          row_expanded(path) ? collapse_row(path) : expand_row(path, false);
        else
          Notebook::get().open(filesystem_path);
      }
    }
  });
  
  signal_test_expand_row().connect([this](const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path){
    if(iter->children().begin()->get_value(column_record.path)=="")
      add_or_update_path(iter->get_value(column_record.path), *iter, true);
    return false;
  });
  signal_row_collapsed().connect([this](const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path){
    this->remove_path(iter->get_value(column_record.path));
  });
  
  enable_model_drag_source();
  enable_model_drag_dest();
  
  auto new_file_label = "New File";
  auto new_file_function = [this] {
    if(menu_popup_row_path.empty())
      return;
    EntryBox::get().clear();
    EntryBox::get().entries.emplace_back("", [this, source_path=menu_popup_row_path](const std::string &content) {
      bool is_directory=boost::filesystem::is_directory(source_path);
      auto target_path = (is_directory ? source_path : source_path.parent_path())/content;
      if(!boost::filesystem::exists(target_path)) {
        if(filesystem::write(target_path, "")) {
          update();
          Notebook::get().open(target_path);
          on_save_file(target_path);
        }
        else {
          Terminal::get().print("Error: could not create "+target_path.string()+'\n', true);
          return;
        }
      }
      else {
        Terminal::get().print("Error: could not create "+target_path.string()+": already exists\n", true);
        return;
      }
      
      EntryBox::get().hide();
    });
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Filename");
    EntryBox::get().buttons.emplace_back("Create New File", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  };
  
  menu_item_new_file.set_label(new_file_label);
  menu_item_new_file.signal_activate().connect(new_file_function);
  menu.append(menu_item_new_file);
  
  menu_root_item_new_file.set_label(new_file_label);
  menu_root_item_new_file.signal_activate().connect(new_file_function);
  menu_root.append(menu_root_item_new_file);
  
  auto new_folder_label = "New Folder";
  auto new_folder_function = [this] {
    if(menu_popup_row_path.empty())
      return;
    EntryBox::get().clear();
    EntryBox::get().entries.emplace_back("", [this, source_path=menu_popup_row_path](const std::string &content) {
      bool is_directory=boost::filesystem::is_directory(source_path);
      auto target_path = (is_directory ? source_path : source_path.parent_path())/content;
      if(!boost::filesystem::exists(target_path)) {
        boost::system::error_code ec;
        boost::filesystem::create_directory(target_path, ec);
        if(!ec) {
          update();
          select(target_path);
        }
        else {
          Terminal::get().print("Error: could not create "+target_path.string()+": "+ec.message(), true);
          return;
        }
      }
      else {
        Terminal::get().print("Error: could not create "+target_path.string()+": already exists\n", true);
        return;
      }
      
      EntryBox::get().hide();
    });
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Folder name");
    EntryBox::get().buttons.emplace_back("Create New Folder", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  };
  
  menu_item_new_folder.set_label(new_folder_label);
  menu_item_new_folder.signal_activate().connect(new_folder_function);
  menu.append(menu_item_new_folder);
  
  menu_root_item_new_folder.set_label(new_folder_label);
  menu_root_item_new_folder.signal_activate().connect(new_folder_function);
  menu_root.append(menu_root_item_new_folder);
  
  menu.append(menu_item_separator);
  
  menu_item_rename.set_label("Rename");
  menu_item_rename.signal_activate().connect([this] {
    if(menu_popup_row_path.empty())
      return;
    EntryBox::get().clear();
    EntryBox::get().entries.emplace_back(menu_popup_row_path.filename().string(), [this, source_path=menu_popup_row_path](const std::string &content){
      bool is_directory=boost::filesystem::is_directory(source_path);
      
      auto target_path=source_path.parent_path()/content;
      
      if(boost::filesystem::exists(target_path)) {
        Terminal::get().print("Error: could not rename to "+target_path.string()+": already exists\n", true);
        return;
      }
      
      if(is_directory)
        this->remove_path(source_path);
      
      boost::system::error_code ec;
      boost::filesystem::rename(source_path, target_path, ec);
      if(ec) {
        Terminal::get().print("Error: could not rename "+source_path.string()+": "+ec.message()+'\n', true);
        return;
      }
      update();
      on_save_file(target_path);
      select(target_path);
      
      for(size_t c=0;c<Notebook::get().size();c++) {
        auto view=Notebook::get().get_view(c);
        if(is_directory) {
          if(filesystem::file_in_path(view->file_path, source_path)) {
            auto file_it=view->file_path.begin();
            for(auto source_it=source_path.begin();source_it!=source_path.end();source_it++)
              file_it++;
            auto new_file_path=target_path;
            for(;file_it!=view->file_path.end();file_it++)
              new_file_path/=*file_it;
            view->rename(new_file_path);
          }
        }
        else if(view->file_path==source_path) {
          view->rename(target_path);
          
          std::string old_language_id;
          if(view->language)
            old_language_id=view->language->get_id();
          view->language=Source::guess_language(target_path);
          std::string new_language_id;
          if(view->language)
            new_language_id=view->language->get_id();
          if(new_language_id!=old_language_id)
            Terminal::get().print("Warning: language for "+target_path.string()+" has changed. Please reopen the file\n");
        }
      }
      
      EntryBox::get().hide();
    });
    
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Filename");
    
    EntryBox::get().buttons.emplace_back("Rename file", [entry_it](){
      entry_it->activate();
    });
    
    EntryBox::get().show();
    
    auto end_pos=Glib::ustring(menu_popup_row_path.filename().string()).rfind('.');
    if(end_pos!=Glib::ustring::npos)
      entry_it->select_region(0, end_pos);
  });
  menu.append(menu_item_rename);
  
  menu_item_delete.set_label("Delete");
  menu_item_delete.signal_activate().connect([this] {
    if(menu_popup_row_path.empty())
      return;
    Gtk::MessageDialog dialog(*static_cast<Gtk::Window*>(get_toplevel()), "Delete!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
    dialog.set_default_response(Gtk::RESPONSE_NO);
    dialog.set_secondary_text("Are you sure you want to delete "+menu_popup_row_path.string()+"?");
    int result = dialog.run();
    if(result==Gtk::RESPONSE_YES) {
      bool is_directory=boost::filesystem::is_directory(menu_popup_row_path);
      
      boost::system::error_code ec;
      boost::filesystem::remove_all(menu_popup_row_path, ec);
      if(ec)
        Terminal::get().print("Error: could not delete "+menu_popup_row_path.string()+": "+ec.message()+"\n", true);
      else {
        update();
        
        for(size_t c=0;c<Notebook::get().size();c++) {
          auto view=Notebook::get().get_view(c);
          
          if(is_directory) {
            if(filesystem::file_in_path(view->file_path, menu_popup_row_path))
              view->get_buffer()->set_modified();
          }
          else if(view->file_path==menu_popup_row_path)
            view->get_buffer()->set_modified();
        }
      }
    }
  });
  menu.append(menu_item_delete);
  
  menu.show_all();
  menu.accelerate(*this);
  
  menu_root.show_all();
  menu_root.accelerate(*this);
  
  set_headers_clickable();
  forall([this](Gtk::Widget &widget) {
    if(widget.get_name()=="GtkButton") {
      widget.signal_button_press_event().connect([this](GdkEventButton *event) {
        if(event->type==GDK_BUTTON_PRESS && event->button==GDK_BUTTON_SECONDARY && !path.empty()) {
          menu_popup_row_path=this->path;
          menu_root.popup(event->button, event->time);
        }
        return true;
      });
    }
  });
}

Directories::~Directories() {
  dispatcher.disconnect();
}

void Directories::open(const boost::filesystem::path &dir_path) {
  boost::system::error_code ec;
  if(dir_path.empty() || !boost::filesystem::exists(dir_path, ec) || ec)
    return;
  
  tree_store->clear();
  
  path=filesystem::get_normal_path(dir_path);
  
  //TODO: report that set_title does not handle '_' correctly?
  auto title=path.filename().string();
  size_t pos=0;
  while((pos=title.find('_', pos))!=std::string::npos) {
    title.replace(pos, 1, "__");
    pos+=2;
  }
  get_column(0)->set_title(title);

  for(auto &directory: directories) {
    if(directory.second.repository)
      directory.second.repository->clear_saved_status();
  }
  directories.clear();
  
  add_or_update_path(path, Gtk::TreeModel::Row(), true);
}

void Directories::update() {
  std::vector<std::pair<std::string, Gtk::TreeModel::Row> > saved_directories;
  for(auto &directory: directories)
    saved_directories.emplace_back(directory.first, directory.second.row);
  for(auto &directory: saved_directories)
    add_or_update_path(directory.first, directory.second, false);
}

void Directories::on_save_file(const boost::filesystem::path &file_path) {
  auto it=directories.find(file_path.parent_path().string());
  if(it!=directories.end()) {
    if(it->second.repository)
      it->second.repository->clear_saved_status();
    colorize_path(it->first, true);
  }
}

void Directories::select(const boost::filesystem::path &select_path) {
  if(path=="")
    return;
  
  if(!filesystem::file_in_path(select_path, path))
    return;
  
  //return if the select_path is already selected
  auto iter=get_selection()->get_selected();
  if(iter) {
    if(iter->get_value(column_record.path)==select_path)
      return;
  }
  
  std::list<boost::filesystem::path> paths;
  boost::filesystem::path parent_path;
  if(boost::filesystem::is_directory(select_path))
    parent_path=select_path;
  else
    parent_path=select_path.parent_path();
  
  //check if select_path is already expanded
  if(directories.find(parent_path.string())!=directories.end()) {
    //set cursor at select_path and return
    tree_store->foreach_iter([this, &select_path](const Gtk::TreeModel::iterator &iter){
      if(iter->get_value(column_record.path)==select_path) {
        auto tree_path=Gtk::TreePath(iter);
        expand_to_path(tree_path);
        set_cursor(tree_path);
        return true;
      }
      return false;
    });
    return;
  }
  
  paths.emplace_front(parent_path);
  while(parent_path!=path) {
    parent_path=parent_path.parent_path();
    paths.emplace_front(parent_path);
  }

  //expand to select_path
  for(auto &a_path: paths) {
    tree_store->foreach_iter([this, &a_path](const Gtk::TreeModel::iterator &iter){
      if(iter->get_value(column_record.path)==a_path) {
        add_or_update_path(a_path, *iter, true);
        return true;
      }
      return false;
    });
  }
  
  //set cursor at select_path
  tree_store->foreach_iter([this, &select_path](const Gtk::TreeModel::iterator &iter){
    if(iter->get_value(column_record.path)==select_path) {
      auto tree_path=Gtk::TreePath(iter);
      expand_to_path(tree_path);
      set_cursor(tree_path);
      return true;
    }
    return false;
  });
}

bool Directories::on_button_press_event(GdkEventButton* event) {
  if(event->type==GDK_BUTTON_PRESS && event->button==GDK_BUTTON_SECONDARY) {
    EntryBox::get().hide();
    Gtk::TreeModel::Path path;
    if(get_path_at_pos(static_cast<int>(event->x), static_cast<int>(event->y), path)) {
      menu_popup_row_path=get_model()->get_iter(path)->get_value(column_record.path);
      if(menu_popup_row_path.empty()) {
        auto parent=get_model()->get_iter(path)->parent();
        if(parent)
          menu_popup_row_path=parent->get_value(column_record.path);
        else {
          menu_popup_row_path=this->path;
          menu_root.popup(event->button, event->time);
          return true;
        }
      }
      menu.popup(event->button, event->time);
      return true;
    }
    else if(!this->path.empty()) {
      menu_popup_row_path=this->path;
      menu_root.popup(event->button, event->time);
      return true;
    }
  }
  
  return Gtk::TreeView::on_button_press_event(event);
}

void Directories::add_or_update_path(const boost::filesystem::path &dir_path, const Gtk::TreeModel::Row &row, bool include_parent_paths) {
  auto path_it=directories.find(dir_path.string());
  if(!boost::filesystem::exists(dir_path)) {
    if(path_it!=directories.end())
      directories.erase(path_it);
    return;
  }
  
  if(path_it==directories.end()) {
    auto g_file=Gio::File::create_for_path(dir_path.string());
    auto monitor=g_file->monitor_directory(Gio::FileMonitorFlags::FILE_MONITOR_WATCH_MOVES);
    auto path_and_row=std::make_shared<std::pair<boost::filesystem::path, Gtk::TreeModel::Row> >(dir_path, row);
    auto connection=std::make_shared<sigc::connection>();
    
    std::shared_ptr<Git::Repository> repository;
    try {
      repository=Git::get_repository(dir_path);
    }
    catch(const std::exception &) {}
    
    monitor->signal_changed().connect([this, connection, path_and_row, repository] (const Glib::RefPtr<Gio::File> &file,
                                                                        const Glib::RefPtr<Gio::File>&,
                                                                        Gio::FileMonitorEvent monitor_event) {
      if(monitor_event!=Gio::FileMonitorEvent::FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
        if(repository)
          repository->clear_saved_status();
        connection->disconnect();
        *connection=Glib::signal_timeout().connect([path_and_row, this]() {
          if(directories.find(path_and_row->first.string())!=directories.end())
            add_or_update_path(path_and_row->first, path_and_row->second, true);
          return false;
        }, 500);
      }
    });
    
    std::shared_ptr<sigc::connection> repository_connection(new sigc::connection(), [](sigc::connection *connection) {
      connection->disconnect();
      delete connection;
    });
    
    if(repository) {
      auto connection=std::make_shared<sigc::connection>();
      *repository_connection=repository->monitor->signal_changed().connect([this, connection, path_and_row](const Glib::RefPtr<Gio::File> &file,
                                                                                                            const Glib::RefPtr<Gio::File>&,
                                                                                                            Gio::FileMonitorEvent monitor_event) {
        if(monitor_event!=Gio::FileMonitorEvent::FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
          connection->disconnect();
          *connection=Glib::signal_timeout().connect([this, path_and_row] {
            if(directories.find(path_and_row->first.string())!=directories.end())
              colorize_path(path_and_row->first, false);
            return false;
          }, 500);
        }
      });
    }
    directories[dir_path.string()]={row, monitor, repository, repository_connection};
  }
  
  Gtk::TreeNodeChildren children(row?row.children():tree_store->children());
  if(children) {
    if(children.begin()->get_value(column_record.path)=="")
      tree_store->erase(children.begin());
  }
  std::unordered_set<std::string> not_deleted;
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(dir_path);it!=end_it;it++) {
    auto filename=it->path().filename().string();
    bool already_added=false;
    if(children) {
      for(auto &child: children) {
        if(child->get_value(column_record.name)==filename) {
          not_deleted.emplace(filename);
          already_added=true;
          break;
        }
      }
    }
    if(!already_added) {
      auto child = tree_store->append(children);
      not_deleted.emplace(filename);
      child->set_value(column_record.name, filename);
      child->set_value(column_record.markup, Glib::Markup::escape_text(filename));
      child->set_value(column_record.path, it->path());
      if (boost::filesystem::is_directory(it->path())) {
        child->set_value(column_record.id, '1'+filename);
        auto grandchild=tree_store->append(child->children());
        grandchild->set_value(column_record.name, std::string("(empty)"));
        grandchild->set_value(column_record.markup, Glib::Markup::escape_text("(empty)"));
        grandchild->set_value(column_record.type, PathType::UNKNOWN);
      }
      else {
        child->set_value(column_record.id, '2'+filename);
        
        auto language=Source::guess_language(it->path().filename());
        if(!language)
          child->set_value(column_record.type, PathType::UNKNOWN);
      }
    }
  }
  if(children) {
    for(auto it=children.begin();it!=children.end();) {
      if(not_deleted.count(it->get_value(column_record.name))==0) {
        it=tree_store->erase(it);
      }
      else
        it++;
    }
  }
  if(!children) {
    auto child=tree_store->append(children);
    child->set_value(column_record.name, std::string("(empty)"));
    child->set_value(column_record.markup, Glib::Markup::escape_text("(empty)"));
    child->set_value(column_record.type, PathType::UNKNOWN);
  }
  
  colorize_path(dir_path, include_parent_paths);
}

void Directories::remove_path(const boost::filesystem::path &dir_path) {
  auto it=directories.find(dir_path.string());
  if(it==directories.end())
    return;
  auto children=it->second.row->children();
  
  for(auto it=directories.begin();it!=directories.end();) {
    if(filesystem::file_in_path(it->first, dir_path))
      it=directories.erase(it);
    else
      it++;
  }
  
  if(children) {
    while(children) {
      tree_store->erase(children.begin());
    }
    auto child=tree_store->append(children);
    child->set_value(column_record.name, std::string("(empty)"));
    child->set_value(column_record.markup, Glib::Markup::escape_text("(empty)"));
    child->set_value(column_record.type, PathType::UNKNOWN);
  }
}

void Directories::colorize_path(boost::filesystem::path dir_path_, bool include_parent_paths) {
  auto dir_path=std::make_shared<boost::filesystem::path>(std::move(dir_path_));
  auto it=directories.find(dir_path->string());
  if(it==directories.end())
    return;
  
  if(it!=directories.end() && it->second.repository) {
    auto repository=it->second.repository;
    std::thread git_status_thread([this, dir_path, repository, include_parent_paths] {
      Git::Repository::Status status;
      try {
        status=repository->get_status();
      }
      catch(const std::exception &e) {
        Terminal::get().async_print(std::string("Error (git): ")+e.what()+'\n', true);
      }
      
      dispatcher.post([this, dir_path, include_parent_paths, status=std::move(status)] {
        auto it=directories.find(dir_path->string());
        if(it==directories.end())
          return;
        
        auto normal_color=get_style_context()->get_color(Gtk::StateFlags::STATE_FLAG_NORMAL);
        Gdk::RGBA gray;
        gray.set_rgba(0.5, 0.5, 0.5);
        Gdk::RGBA yellow;
        yellow.set_rgba(1.0, 1.0, 0.2);
        double factor=0.5;
        yellow.set_red(normal_color.get_red()+factor*(yellow.get_red()-normal_color.get_red()));
        yellow.set_green(normal_color.get_green()+factor*(yellow.get_green()-normal_color.get_green()));
        yellow.set_blue(normal_color.get_blue()+factor*(yellow.get_blue()-normal_color.get_blue()));
        Gdk::RGBA green;
        green.set_rgba(0.0, 1.0, 0.0);
        factor=0.4;
        green.set_red(normal_color.get_red()+factor*(green.get_red()-normal_color.get_red()));
        green.set_green(normal_color.get_green()+factor*(green.get_green()-normal_color.get_green()));
        green.set_blue(normal_color.get_blue()+factor*(green.get_blue()-normal_color.get_blue()));
        
        do {
          Gtk::TreeNodeChildren children(it->second.row?it->second.row.children():tree_store->children());
          if(!children)
            return;
          
          for(auto &child: children) {
            auto name=Glib::Markup::escape_text(child.get_value(column_record.name));
            auto path=child.get_value(column_record.path);
            Gdk::RGBA *color;
            if(status.modified.find(path.generic_string())!=status.modified.end())
              color=&yellow;
            else if(status.added.find(path.generic_string())!=status.added.end())
              color=&green;
            else
              color=&normal_color;
            
            std::stringstream ss;
            ss << '#' << std::setfill('0') << std::hex;
            ss << std::setw(2) << std::hex << (color->get_red_u()>>8);
            ss << std::setw(2) << std::hex << (color->get_green_u()>>8);
            ss << std::setw(2) << std::hex << (color->get_blue_u()>>8);
            child.set_value(column_record.markup, "<span foreground=\""+ss.str()+"\">"+name+"</span>");
            
            auto type=child.get_value(column_record.type);
            if(type==PathType::UNKNOWN)
              child.set_value(column_record.markup, "<i>"+child.get_value(column_record.markup)+"</i>");
          }
          
          if(!include_parent_paths)
            break;
          
          auto path=boost::filesystem::path(it->first);
          if(boost::filesystem::exists(path/".git"))
            break;
          if(path==path.root_directory())
            break;
          auto parent_path=boost::filesystem::path(it->first).parent_path();
          it=directories.find(parent_path.string());
        } while(it!=directories.end());
      });
    });
    git_status_thread.detach();
  }
}
