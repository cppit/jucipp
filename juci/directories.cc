#include "directories.h"
#include "logging.h"

Directories::Controller::Controller(Directories::Config& cfg) :
  config_(cfg) {
  m_ScrolledWindow.add(m_TreeView);
  m_ScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
}

void Directories::Controller::
open_folder(const boost::filesystem::path& dir_path) {
  INFO("Open folder");
  m_refTreeModel = Gtk::TreeStore::create(view());
  m_TreeView.set_model(m_refTreeModel);
  m_TreeView.remove_all_columns();
  std::string project_name = GetCmakeVarValue(dir_path, "project");
  m_TreeView.append_column(project_name, view().m_col_name);
  int row_id = 0;
  Gtk::TreeModel::Row row;
  list_dirs(dir_path, row, row_id);
  m_refTreeModel->set_sort_column(0, Gtk::SortType::SORT_ASCENDING);
  INFO("Folder opened");
}

bool Directories::Controller::IsIgnored(std::string path) {
  std::transform(path.begin(), path.end(), path.begin(), ::tolower);
  //  std::cout << "ignored?: " << path << std::endl;
  if (config().IsException(path)) {
    return false;
  }
  if (config().IsIgnored(path)) {
    return true;
  }
  return false;
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
    if (!IsIgnored(itr->path().filename().string())) {
      if (boost::filesystem::is_directory(itr->status())) {
        if (count(itr->path().string()) > count(dir_path.string())) {  // is child
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
}
int Directories::Controller::count(const std::string path) {
  int count = 0;
  for (int i = 0; i < path.size(); i++)
    if (path[i] == '/') count++;
  return count;
}

  std::string Directories::Controller::
  GetCmakeVarValue(const boost::filesystem::path& dir_path, std::string command_name) {
    INFO("fetches cmake variable value for: "+command_name);
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
        std::ifstream ifs2(itr->path().string());
        while (std::getline(ifs2, line)) {
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

  Directories::Config::Config() {
  }
  Directories::Config::Config(Directories::Config& cfg) :
    ignore_list_(cfg.ignore_list()), exception_list_(cfg.exception_list()) {
  }

  void Directories::Config::AddIgnore(std::string filter) {
    ignore_list_.push_back(filter);
  }

  void Directories::Config::AddException(std::string filter) {
    exception_list_.push_back(filter);
  }

  bool Directories::Config::IsIgnored(std::string str) {
    for ( auto &i : ignore_list() )
      if (str.find(i, 0) != std::string::npos)
        return true;
    return false;
  }

  bool Directories::Config::IsException(std::string str) {
    for ( std::string &i : exception_list() )
      if (i == str)
        return true;
    return false;
  }
