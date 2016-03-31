#include "directories.h"
#include "logging.h"
#include <algorithm>
#include <unordered_set>
#include "source.h"
#include "terminal.h"
#include "notebook.h"
#include "filesystem.h"
#include "entrybox.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
#else
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#endif
}

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
    auto target_path=get_target_folder(path);
    
    target_path/=source_path.filename();
    
    if(source_path==target_path)
      return false;
    
    if(boost::filesystem::exists(target_path)) {
      Terminal::get().print("Error: could not move file: "+target_path.string()+" already exists\n", true);
      return false;
    }
    
    bool is_directory=boost::filesystem::is_directory(source_path);
    
    boost::system::error_code ec;
    boost::filesystem::rename(source_path, target_path, ec);
    if(ec) {
      Terminal::get().print("Error: could not move file: "+ec.message()+'\n', true);
      return false;
    }
    
    for(int c=0;c<Notebook::get().size();c++) {
      auto view=Notebook::get().get_view(c);
      if(is_directory) {
        if(filesystem::file_in_path(view->file_path, source_path)) {
          auto file_it=view->file_path.begin();
          for(auto source_it=source_path.begin();source_it!=source_path.end();source_it++)
            file_it++;
          auto new_file_path=target_path;
          for(;file_it!=view->file_path.end();file_it++)
            new_file_path/=*file_it;
          view->file_path=new_file_path;
          g_signal_emit_by_name(view->get_buffer()->gobj(), "modified_changed");
        }
      }
      else if(view->file_path==source_path) {
        view->file_path=target_path;
        g_signal_emit_by_name(view->get_buffer()->gobj(), "modified_changed");
        break;
      }
    }
    
    Directories::get().update();
    directories.select(target_path);
  }
  
  return false;
}

bool Directories::TreeStore::drag_data_delete_vfunc (const Gtk::TreeModel::Path &path) {
  return false;
}

Directories::Directories() : Gtk::TreeView(), stop_update_thread(false) {
  this->set_enable_tree_lines(true);
  
  tree_store = TreeStore::create();
  tree_store->set_column_types(column_record);
  set_model(tree_store);
  append_column("", column_record.name);
  auto renderer=dynamic_cast<Gtk::CellRendererText*>(get_column(0)->get_first_cell());
  get_column(0)->add_attribute(renderer->property_foreground_rgba(), column_record.color);
  
  tree_store->set_sort_column(column_record.id, Gtk::SortType::SORT_ASCENDING);
  set_enable_search(true); //TODO: why does this not work in OS X?
  set_search_column(column_record.name);
  
  signal_row_activated().connect([this](const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *column){
    auto iter = tree_store->get_iter(path);
    if (iter) {
      auto filesystem_path=iter->get_value(column_record.path);
      if(filesystem_path!="") {
        if (boost::filesystem::is_directory(boost::filesystem::path(filesystem_path))) {
          row_expanded(path) ? collapse_row(path) : expand_row(path, false);
        } else {
          if(on_row_activated)
            on_row_activated(filesystem_path);
        }
      }
    }
  });
  
  signal_test_expand_row().connect([this](const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path){
    if(iter->children().begin()->get_value(column_record.path)=="") {
      update_mutex.lock();
      add_path(iter->get_value(column_record.path), *iter);
      update_mutex.unlock();
    }
    return false;
  });
  signal_row_collapsed().connect([this](const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path){
    update_mutex.lock();
    auto directory_str=iter->get_value(column_record.path).string();
    for(auto it=last_write_times.begin();it!=last_write_times.end();) {
      if(directory_str==it->first.substr(0, directory_str.size()))
        it=last_write_times.erase(it);
      else
        it++;
    }
    update_mutex.unlock();
    auto children=iter->children();
    if(children) {
      while(children) {
        tree_store->erase(children.begin());
      }
      auto child=tree_store->append(iter->children());
      child->set_value(column_record.name, std::string("(empty)"));
      Gdk::RGBA rgba;
      rgba.set_rgba(0.5, 0.5, 0.5);
      child->set_value(column_record.color, rgba);
    }
  });
    
  update_thread=std::thread([this](){
    while(!stop_update_thread) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      update_mutex.lock();
      for(auto it=last_write_times.begin();it!=last_write_times.end();) {
        boost::system::error_code ec;
        auto last_write_time=boost::filesystem::last_write_time(it->first, ec);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        if(!ec) {
          if(last_write_time!=now && it->second.second<last_write_time) {
            auto path=std::make_shared<std::string>(it->first);
            dispatcher.post([this, path, last_write_time] {
              update_mutex.lock();
              auto it=last_write_times.find(*path);
              if(it!=last_write_times.end())
                add_path(*path, it->second.first, last_write_time);
              update_mutex.unlock();
            });
          }
          it++;
        }
        else
          it=last_write_times.erase(it);
      }
      update_mutex.unlock();
    }
  });
  
  enable_model_drag_source();
  enable_model_drag_dest();
  
  menu_item_rename.set_label("Rename");
  menu_item_rename.signal_activate().connect([this] {
    if(menu_popup_row_path.empty())
      return;
    EntryBox::get().clear();
    auto source_path=std::make_shared<boost::filesystem::path>(menu_popup_row_path);
    EntryBox::get().entries.emplace_back(menu_popup_row_path.filename().string(), [this, source_path](const std::string &content){
      bool is_directory=boost::filesystem::is_directory(*source_path);
      
      boost::system::error_code ec;
      auto target_path=source_path->parent_path()/content;
      boost::filesystem::rename(*source_path, target_path, ec);
      if(ec)
        Terminal::get().print("Error: could not rename "+source_path->string()+": "+ec.message()+'\n');
      else {
        update();
        select(target_path);
        
        for(int c=0;c<Notebook::get().size();c++) {
          auto view=Notebook::get().get_view(c);
          if(is_directory) {
            if(filesystem::file_in_path(view->file_path, *source_path)) {
              auto file_it=view->file_path.begin();
              for(auto source_it=source_path->begin();source_it!=source_path->end();source_it++)
                file_it++;
              auto new_file_path=target_path;
              for(;file_it!=view->file_path.end();file_it++)
                new_file_path/=*file_it;
              view->file_path=new_file_path;
            }
          }
          else if(view->file_path==*source_path) {
            view->file_path=target_path;
            g_signal_emit_by_name(view->get_buffer()->gobj(), "modified_changed");
            
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
      }
    });
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Filename");
    EntryBox::get().buttons.emplace_back("Rename file", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.append(menu_item_rename);
  
  menu_item_delete.set_label("Delete");
  menu_item_delete.signal_activate().connect([this] {
    if(menu_popup_row_path.empty())
      return;
    Gtk::MessageDialog dialog((Gtk::Window&)(*get_toplevel()), "Delete!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
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
        
        for(int c=0;c<Notebook::get().size();c++) {
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
}

Directories::~Directories() {
  stop_update_thread=true;
  update_thread.join();
  dispatcher.disconnect();
}

void Directories::open(const boost::filesystem::path &dir_path) {
  JDEBUG("start");
  if(dir_path.empty())
    return;
  
  tree_store->clear();
  update_mutex.lock();
  last_write_times.clear();
  update_mutex.unlock();
    
  
  //TODO: report that set_title does not handle '_' correctly?
  auto title=dir_path.filename().string();
  size_t pos=0;
  while((pos=title.find('_', pos))!=std::string::npos) {
    title.replace(pos, 1, "__");
    pos+=2;
  }
  get_column(0)->set_title(title);

  update_mutex.lock();
  add_path(dir_path, Gtk::TreeModel::Row());
  update_mutex.unlock();
    
  path=dir_path;
  
 JDEBUG("end");
}

void Directories::update() {
 JDEBUG("start");
  update_mutex.lock();
  for(auto &last_write_time: last_write_times) {
    add_path(last_write_time.first, last_write_time.second.first);
  }
  update_mutex.unlock();
 JDEBUG("end");
}

void Directories::select(const boost::filesystem::path &select_path) {
 JDEBUG("start");
  if(path=="")
    return;
    
  if(select_path.generic_string().substr(0, path.generic_string().size()+1)!=path.generic_string()+'/')
    return;
  
  std::list<boost::filesystem::path> paths;
  boost::filesystem::path parent_path;
  if(boost::filesystem::is_directory(select_path))
    parent_path=select_path;
  else
    parent_path=select_path.parent_path();
  paths.emplace_front(parent_path);
  while(parent_path!=path) {
    parent_path=parent_path.parent_path();
    paths.emplace_front(parent_path);
  }

  for(auto &a_path: paths) {
    tree_store->foreach_iter([this, &a_path](const Gtk::TreeModel::iterator &iter){
      if(iter->get_value(column_record.path)==a_path) {
        update_mutex.lock();
        add_path(a_path, *iter);
        update_mutex.unlock();
        return true;
      }
      return false;
    });
  }
  
  tree_store->foreach_iter([this, &select_path](const Gtk::TreeModel::iterator &iter){
    if(iter->get_value(column_record.path)==select_path) {
      auto tree_path=Gtk::TreePath(iter);
      expand_to_path(tree_path);
      set_cursor(tree_path);
      return true;
    }
    return false;
  });
 JDEBUG("end");
}

bool Directories::on_button_press_event(GdkEventButton* event) {
  if(event->type==GDK_BUTTON_PRESS && event->button==GDK_BUTTON_SECONDARY) {
    Gtk::TreeModel::Path path;
    if(get_path_at_pos(static_cast<int>(event->x), static_cast<int>(event->y), path)) {
      menu_popup_row_path=get_model()->get_iter(path)->get_value(column_record.path);
      menu.popup(event->button, event->time);
      return true;
    }
  }
  
  return Gtk::TreeView::on_button_press_event(event);
}

void Directories::add_path(const boost::filesystem::path &dir_path, const Gtk::TreeModel::Row &parent, time_t last_write_time) {
  boost::system::error_code ec;
  if(last_write_time==0)
    last_write_time=boost::filesystem::last_write_time(dir_path, ec);
  if(ec)
    return;
  last_write_times[dir_path.string()]={parent, last_write_time};
  std::unique_ptr<Gtk::TreeNodeChildren> children; //Gtk::TreeNodeChildren is missing default constructor...
  if(parent)
    children=std::unique_ptr<Gtk::TreeNodeChildren>(new Gtk::TreeNodeChildren(parent.children()));
  else
    children=std::unique_ptr<Gtk::TreeNodeChildren>(new Gtk::TreeNodeChildren(tree_store->children()));
  if(*children) {
    if(children->begin()->get_value(column_record.path)=="")
      tree_store->erase(children->begin());
  }
  std::unordered_set<std::string> not_deleted;
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(dir_path);it!=end_it;it++) {
    auto filename=it->path().filename().string();
    bool already_added=false;
    if(*children) {
      for(auto &child: *children) {
        if(child->get_value(column_record.name)==filename) {
          not_deleted.emplace(filename);
          already_added=true;
          break;
        }
      }
    }
    if(!already_added) {
      auto child = tree_store->append(*children);
      not_deleted.emplace(filename);
      child->set_value(column_record.name, filename);
      child->set_value(column_record.path, it->path());
      if (boost::filesystem::is_directory(it->path())) {
        child->set_value(column_record.id, "a"+filename);
        auto grandchild=tree_store->append(child->children());
        grandchild->set_value(column_record.name, std::string("(empty)"));
        Gdk::RGBA rgba;
        rgba.set_rgba(0.5, 0.5, 0.5);
        grandchild->set_value(column_record.color, rgba);
      }
      else {
        child->set_value(column_record.id, "b"+filename);
        
        auto language=Source::guess_language(it->path().filename());
        if(!language) {
          Gdk::RGBA rgba;
          rgba.set_rgba(0.5, 0.5, 0.5);
          child->set_value(column_record.color, rgba);
        }
      }
    }
  }
  if(*children) {
    for(auto it=children->begin();it!=children->end();) {
      if(not_deleted.count(it->get_value(column_record.name))==0) {
        it=tree_store->erase(it);
      }
      else
        it++;
    }
  }
  if(!*children) {
    auto child=tree_store->append(*children);
    child->set_value(column_record.name, std::string("(empty)"));
    Gdk::RGBA rgba;
    rgba.set_rgba(0.5, 0.5, 0.5);
    child->set_value(column_record.color, rgba);
  }
}
