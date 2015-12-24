#include "directories.h"
#include "logging.h"
#include <algorithm>
#include <unordered_set>
#include "source.h"

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

Directories::Directories() : Gtk::TreeView(), stop_update_thread(false) {
  tree_store = Gtk::TreeStore::create(column_record);
  set_model(tree_store);
  append_column("", column_record.name);
  auto renderer=dynamic_cast<Gtk::CellRendererText*>(get_column(0)->get_first_cell());
  get_column(0)->add_attribute(renderer->property_foreground_rgba(), column_record.color);
  
  tree_store->set_sort_column(column_record.id, Gtk::SortType::SORT_ASCENDING);
  set_enable_search(true); //TODO: why does this not work in OS X?
  set_search_column(column_record.name);
  
  signal_row_activated().connect([this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column){
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
  
  signal_test_expand_row().connect([this](const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path){
    if(iter->children().begin()->get_value(column_record.path)=="") {
      update_mutex.lock();
      add_path(iter->get_value(column_record.path), *iter);
      update_mutex.unlock();
    }
    return false;
  });
  signal_row_collapsed().connect([this](const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path){
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
  
  update_dispatcher.connect([this](){
    update_mutex.lock();
    for(auto &path: update_paths) {
      if(last_write_times.count(path)>0)
        add_path(path, last_write_times.at(path).first);
    }
    update_paths.clear();
    update_mutex.unlock();
  });
  
  update_thread=std::thread([this](){
    while(!stop_update_thread) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      update_mutex.lock();
      if(update_paths.size()==0) {
        for(auto it=last_write_times.begin();it!=last_write_times.end();) {
          boost::system::error_code ec;
          auto last_write_time=boost::filesystem::last_write_time(it->first, ec);
          if(!ec) {
            if(it->second.second<last_write_time) {
              update_paths.emplace_back(it->first);
            }
            it++;
          }
          else
            it=last_write_times.erase(it);
        }
        if(update_paths.size()>0)
          update_dispatcher();
      }
      update_mutex.unlock();
    }
  });
}

Directories::~Directories() {
  stop_update_thread=true;
  update_thread.join();
}

void Directories::open(const boost::filesystem::path& dir_path) {
  JDEBUG("start");
  if(dir_path.empty())
    return;
  
  tree_store->clear();
  update_mutex.lock();
  last_write_times.clear();
  update_paths.clear();
  update_mutex.unlock();
    
  cmake=std::unique_ptr<CMake>(new CMake(dir_path));
  auto project=cmake->get_functions_parameters("project");
  if(project.size()>0 && project[0].second.size()>0)
    get_column(0)->set_title(project[0].second[0]);
  else
    get_column(0)->set_title("");
  update_mutex.lock();
  add_path(dir_path, Gtk::TreeModel::Row());
  update_mutex.unlock();
    
  current_path=dir_path;
  
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

void Directories::select(const boost::filesystem::path &path) {
 JDEBUG("start");
  if(current_path=="")
    return;
    
  if(path.generic_string().substr(0, current_path.generic_string().size()+1)!=current_path.generic_string()+'/')
    return;
  
  std::list<boost::filesystem::path> paths;
  boost::filesystem::path parent_path;
  if(boost::filesystem::is_directory(path))
    parent_path=path;
  else
    parent_path=path.parent_path();
  paths.emplace_front(parent_path);
  while(parent_path!=current_path) {
    parent_path=parent_path.parent_path();
    paths.emplace_front(parent_path);
  }

  for(auto &a_path: paths) {
    tree_store->foreach_iter([this, &a_path](const Gtk::TreeModel::iterator& iter){
      if(iter->get_value(column_record.path)==a_path) {
        update_mutex.lock();
        add_path(a_path, *iter);
        update_mutex.unlock();
        return true;
      }
      return false;
    });
  }
  
  tree_store->foreach_iter([this, &path](const Gtk::TreeModel::iterator& iter){
    if(iter->get_value(column_record.path)==path) {
      auto tree_path=Gtk::TreePath(iter);
      expand_to_path(tree_path);
      set_cursor(tree_path);
      return true;
    }
    return false;
  });
 JDEBUG("end");
}

void Directories::add_path(const boost::filesystem::path& dir_path, const Gtk::TreeModel::Row &parent) {
  boost::system::error_code ec;
  auto last_write_time=boost::filesystem::last_write_time(dir_path, ec);
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
