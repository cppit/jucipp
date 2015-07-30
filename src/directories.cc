#include "directories.h"
#include "sourcefile.h"
#include "logging.h"
#include "singletons.h"
#include <algorithm>
#include "boost/algorithm/string.hpp"

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
  INFO("Open folder");
  tree_store->clear();
  tree_view.get_column(0)->set_title(get_cmakelists_variable(dir_path, "project"));
  add_paths(dir_path, Gtk::TreeModel::Row(), 0);
  DEBUG("Folder opened");
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

std::string Directories::get_cmakelists_variable(const boost::filesystem::path& dir_path, std::string command_name) {
  INFO("fetches cmake variable value for: "+command_name);
  std::string project_name;
  std::string project_name_var;
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr( dir_path );itr != end_itr;++itr ) {
    if (itr->path().filename().string() == "CMakeLists.txt") {
      for (auto &line : juci::filesystem::lines(itr->path())) {
        if (line.find(command_name+"(", 0) != std::string::npos
            || line.find(command_name+" (", 0) != std::string::npos ) {
          size_t variable_start = line.find("{", 0);
          size_t variable_end = line.find("}", variable_start);
          project_name_var = line.substr(variable_start+1,
                                         (variable_end)-variable_start-1);
          boost::algorithm::trim(project_name_var);
          if (variable_start == std::string::npos) { //  not a variabel
            variable_start = line.find("(", 0);
	      
            variable_end = line.find(' ', variable_start);
            if(variable_end != std::string::npos){
              return line.substr(variable_start+1,
                                 (variable_end)-variable_start-1);
            }
            variable_end = line.find("#", variable_start);
            if(variable_end != std::string::npos){
              return line.substr(variable_start+1,
                                 (variable_end)-variable_start-1);
            }
            variable_end = line.find(")", variable_start);
            return line.substr(variable_start+1,
                               (variable_end)-variable_start-1);
            if (variable_start == std::string::npos) { //  not a variable
              variable_start = line.find("(", 0);
              variable_end = line.find(")", variable_start);
              INFO("Wasn't a variable, returning value");
              return line.substr(variable_start+1,
                                 (variable_end)-variable_start-1);
            }
            break;
          }
        }
      }
      for (auto &line : juci::filesystem::lines(itr->path())) {
        if (line.find("set(", 0) != std::string::npos
            || line.find("set (", 0) != std::string::npos) {
          if( line.find(project_name_var, 0) != std::string::npos) {
            size_t variable_start = line.find(project_name_var, 0)
              +project_name_var.length();
            size_t variable_end = line.find(")", variable_start);
            project_name = line.substr(variable_start+1,
                                       variable_end-variable_start-1);
            boost::algorithm::trim(project_name);
            INFO("found variable, returning value");
            return project_name;
          }
        }
      }
      break;
    }
  }
  INFO("Couldn't find value in CMakeLists.txt");
  return "no project name";
}
