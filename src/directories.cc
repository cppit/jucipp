#include "directories.h"
#include "sourcefile.h"
#include "logging.h"
#include "singletons.h"
#include <algorithm>
#include <unordered_set>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Directories::Directories() {
  DEBUG("adding treeview to scrolledwindow");
  add(tree_view);
  set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  tree_store = Gtk::TreeStore::create(column_record);
  tree_view.set_model(tree_store);
  tree_view.append_column("", column_record.name);
  tree_store->set_sort_column(column_record.id, Gtk::SortType::SORT_ASCENDING);
  
  tree_view.signal_row_activated().connect([this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column){
    INFO("Directory navigation");
    auto iter = tree_store->get_iter(path);
    if (iter) {
      auto path_str=iter->get_value(column_record.path);
      if (boost::filesystem::is_directory(boost::filesystem::path(path_str))) {
        tree_view.row_expanded(path) ? tree_view.collapse_row(path) : tree_view.expand_row(path, false);
      } else {
        if(on_row_activated)
          on_row_activated(path_str);
      }
    }
  });
  
  tree_view.signal_test_expand_row().connect([this](const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path){
    if(iter->children().begin()->get_value(column_record.path)=="") {
      add_path(iter->get_value(column_record.path), *iter);
    }
    return false;
  });
  tree_view.signal_row_collapsed().connect([this](const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path){
    auto children=iter->children();
    if(children) {
      while(children) {
        tree_store->erase(children.begin());
      }
      tree_store->append(iter->children());
    }
  });
}

void Directories::open_folder(const boost::filesystem::path& dir_path) {
  if(dir_path!="")
    tree_store->clear();
  
  auto new_path=dir_path;
  INFO("Open folder");
  if(dir_path=="") {
    if(current_path=="")
      return;
    new_path=current_path;
  }
  
  std::vector<Gtk::TreeModel::Path> expanded_paths;
  if(current_path==new_path) {
    tree_view.map_expanded_rows([&expanded_paths](Gtk::TreeView* tree_view, const Gtk::TreeModel::Path& path){
      expanded_paths.emplace_back(path);
    });
  }
  
  if(dir_path!="")
    cmake=std::unique_ptr<CMake>(new CMake(new_path));
  auto project=cmake->get_functions_parameters("project");
  if(project.size()>0 && project[0].second.size()>0)
    tree_view.get_column(0)->set_title(project[0].second[0]);
  else
    tree_view.get_column(0)->set_title("");
  add_path(new_path, Gtk::TreeModel::Row());

  for(auto &path: expanded_paths)
    tree_view.expand_row(path, false);
    
  current_path=new_path;
  
  DEBUG("Folder opened");
}

void Directories::select_path(const boost::filesystem::path &path) {
  if(current_path=="")
    return;
    
  if(path.string().substr(0, current_path.string().size())!=current_path.string())
    return;
  
  if(boost::filesystem::is_directory(path))
    return;
    
  std::list<boost::filesystem::path> paths;
  auto parent_path=path.parent_path();
  paths.emplace_front(parent_path);
  while(parent_path!=current_path) {
    parent_path=parent_path.parent_path();
    paths.emplace_front(parent_path);
  }

  for(auto &a_path: paths) {
    tree_store->foreach_iter([this, &a_path](const Gtk::TreeModel::iterator& iter){
      if(iter->get_value(column_record.path)==a_path.string()) {
        add_path(a_path, *iter);
        return true;
      }
      return false;
    });
  }
  
  tree_store->foreach_iter([this, &path](const Gtk::TreeModel::iterator& iter){
    if(iter->get_value(column_record.path)==path.string()) {
      auto tree_path=Gtk::TreePath(iter);
      tree_view.expand_to_path(tree_path);
      tree_view.set_cursor(tree_path);
      selected_path=path;
      return true;
    }
    return false;
  });
}

bool Directories::ignored(std::string path) {
  DEBUG("Checking if file-/directory is filtered");
  std::transform(path.begin(), path.end(), path.begin(), ::tolower);
  
  for(std::string &i : Singleton::Config::directories()->exceptions) {
    if(i == path)
      return false;
  }
  for(auto &i : Singleton::Config::directories()->ignored) {
    if(path.find(i, 0) != std::string::npos)
      return true;
  }
  
  return false;
}

void Directories::add_path(const boost::filesystem::path& dir_path, const Gtk::TreeModel::Row &parent) {
  auto children=tree_store->children();
  if(parent)
    children=parent.children();
  if(children) {
    if(children.begin()->get_value(column_record.path)=="") {
      tree_store->erase(parent->children().begin());
    }
  }
  std::unordered_set<std::string> not_deleted;
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(dir_path);it!=end_it;it++) {
    auto filename=it->path().filename().string();
    if (!ignored(filename)) {
      bool already_added=false;
      if(children) {
        for(auto &child: children) {
          if(child.get_value(column_record.name)==filename) {
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
        child->set_value(column_record.path, it->path().string());
        if (boost::filesystem::is_directory(*it)) {
          child->set_value(column_record.id, "a"+filename);
          tree_store->append(child->children());
        }
        else
          child->set_value(column_record.id, "b"+filename);
      }
    }
  }
  if(children) {
    auto last_it=children.begin();
    for(auto it=children.begin();it!=children.end();it++) {
      if(not_deleted.count(it->get_value(column_record.name))==0) {
        tree_store->erase(it);
        it=last_it;
      }
      last_it=it;
    }
  }
  else
    tree_store->append(children);
}
