#include "directories.h"
#include "sourcefile.h"
#include "logging.h"
#include "singletons.h"
#include <algorithm>
#include "boost/algorithm/string.hpp"

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
  tree_store->set_sort_column(0, Gtk::SortType::SORT_ASCENDING);
  
  tree_view.signal_row_activated().connect([this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column){
    INFO("Directory navigation");
    auto iter = tree_store->get_iter(path);
    if (iter) {
      Gtk::TreeModel::Row row = *iter;
      std::string upath = Glib::ustring(row[column_record.path]);
      boost::filesystem::path fs_path(upath);
      if (boost::filesystem::is_directory(fs_path)) {
        tree_view.row_expanded(path) ? tree_view.collapse_row(path) : tree_view.expand_row(path, false);
      } else {
        std::stringstream sstm;
        sstm << row[column_record.path];
        if(on_row_activated)
          on_row_activated(sstm.str());
      }
    }
  });
}

void Directories::open_folder(const boost::filesystem::path& dir_path) {
  auto new_path=dir_path;
  INFO("Open folder");
  if(new_path=="") {
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
  
  tree_store->clear();
  
  if(dir_path!="")
    cmake=std::unique_ptr<CMake>(new CMake(new_path));
  auto project=cmake->get_functions_parameters("project");
  if(project.size()>0 && project[0].second.size()>0)
    tree_view.get_column(0)->set_title(project[0].second[0]);
  else
    tree_view.get_column(0)->set_title("");
  add_paths(new_path, Gtk::TreeModel::Row(), 0);

  for(auto &path: expanded_paths)
    tree_view.expand_row(path, false);
    
  current_path=new_path;
  
  if(selected_path.size()>0)
    select_path(selected_path);
  DEBUG("Folder opened");
}

void Directories::select_path(const std::string &path) {
  tree_store->foreach_iter([this, &path](const Gtk::TreeModel::iterator& iter){
    if(iter->get_value(column_record.path)==path) {
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

void Directories::add_paths(const boost::filesystem::path& dir_path, const Gtk::TreeModel::Row &parent, unsigned row_id) {
  boost::filesystem::directory_iterator end_itr;
  Gtk::TreeModel::Row child;
  Gtk::TreeModel::Row row;
  DEBUG("");
  // Fill the treeview
  for(boost::filesystem::directory_iterator itr(dir_path);itr != end_itr;++itr) {   
    if (!ignored(itr->path().filename().string())) {
      if (boost::filesystem::is_directory(itr->status())) {
        if (boost::filesystem::canonical(itr->path()) > boost::filesystem::canonical(dir_path)) {  // is child
          child = *(tree_store->append(parent.children()));
          std::string col_id("a"+itr->path().filename().string());
          child[column_record.id] = col_id;
          child[column_record.name] =  itr->path().filename().string();
          child[column_record.path] = itr->path().string();
          add_paths(itr->path(), child, row_id);
        } else {
          row = *(tree_store->append());
          std::string col_id("a"+itr->path().filename().string());
          row[column_record.path] = itr->path().string();
          row[column_record.id] = col_id;
          row[column_record.name] =  itr->path().filename().string();
          add_paths(itr->path(), parent, row_id);
        }
      } else {  // is a file
        child = *(tree_store->append(parent.children()));
        std::string col_id("b"+itr->path().filename().string());
        child[column_record.id] = col_id;
        child[column_record.name] = itr->path().filename().string();
        child[column_record.path] = itr->path().string();
      }
    }
  }
}
