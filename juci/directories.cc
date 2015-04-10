#include "directories.h"

Directories::Controller::Controller() {
  m_ScrolledWindow.add(m_TreeView);
  m_ScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
}

bool Directories::Controller::
open_folder(const boost::filesystem::path& dir_path) {
  m_refTreeModel = Gtk::TreeStore::create(view());
  m_TreeView.set_model(m_refTreeModel);
  m_TreeView.remove_all_columns();
  std::string project_name = get_project_name(dir_path);
  m_TreeView.append_column(project_name, view().m_col_name);
  int row_id = 0;
  Gtk::TreeModel::Row row;
  list_dirs(dir_path, row, row_id);
  m_refTreeModel->set_sort_column(0, Gtk::SortType::SORT_ASCENDING);
}

void Directories::Controller::
list_dirs(const boost::filesystem::path& dir_path,
          Gtk::TreeModel::Row &parent,
          unsigned row_id) { 
  boost::filesystem::directory_iterator end_itr;
  unsigned dir_counter = row_id;
  unsigned file_counter = 0;
  Gtk::TreeModel::Row child;
  Gtk::TreeModel::Row row;

  // Fill the treeview
  for ( boost::filesystem::directory_iterator itr( dir_path );
        itr != end_itr;
        ++itr ) {
    if (boost::filesystem::is_directory(itr->status())) {
      if (count(itr->path().string()) > count(dir_path.string())) {
        child = *(m_refTreeModel->append(parent.children()));
        std::string col_id("a"+itr->path().filename().string());
        child[view().m_col_id] = col_id;
        child[view().m_col_name] =  itr->path().filename().string();
        child[view().m_col_path] = itr->path().string();
        list_dirs(itr->path(), child, row_id);
      } else {
        row = *(m_refTreeModel->append());
        std::string col_id("a"+itr->path().filename().string());
        row[view().m_col_path] = itr->path().string();
        row[view().m_col_id] = col_id;
        row[view().m_col_name] =  itr->path().filename().string();
        list_dirs(itr->path(), parent, row_id);
      }
    } else {  // is a file
      child = *(m_refTreeModel->append(parent.children()));
      std::string col_id("b"+itr->path().filename().string());
      child[view().m_col_id] = col_id;
      child[view().m_col_name] = itr->path().filename().string();
      child[view().m_col_path] = itr->path().string();
    }
  }
}
int Directories::Controller::count(const std::string path) {
  int count = 0;
  for (int i = 0; i < path.size(); i++)
    if (path[i] == '/') count++;
  return count;
}
std::string Directories::Controller::
get_project_name(const boost::filesystem::path& dir_path) {
  std::string project_name;
  std::string project_name_var;
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr( dir_path );
       itr != end_itr;
       ++itr ) {
    if (itr->path().filename().string() == "CMakeLists.txt") {
      std::ifstream ifs(itr->path().string());
      std::string line;
      while (std::getline(ifs, line)) {
        if (line.find("project(", 0) != std::string::npos
            || line.find("project (", 0) != std::string::npos ) {
          size_t variabel_start = line.find("{", 0);
          size_t variabel_end = line.find("}", variabel_start);
          project_name_var = line.substr(variabel_start+1,
                                         (variabel_end)-variabel_start-1);
          break;
        }
      }
      std::ifstream ifs2(itr->path().string());
      while (std::getline(ifs2, line)) {
        if (line.find("set("+project_name_var, 0) != std::string::npos) {
          size_t variabel_start = line.find(project_name_var, 0)
            +project_name_var.length();
          size_t variabel_end = line.find(")", variabel_start);
          project_name = line.substr(variabel_start,
                                     variabel_end-variabel_start);
          return project_name;
        }
      }
      break;
    }
  }
  return "no project name";
}

